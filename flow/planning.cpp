#include "planning.h"

namespace humanus {

// Get an appropriate executor agent for the current step.
// Can be extended to select agents based on step type/requirements.
std::shared_ptr<BaseAgent> PlanningFlow::get_executor(const std::string& step_type) const {
    // If step type is provided and matches an agent key, use that agent
    if (!step_type.empty() && agents.find(step_type) != agents.end()) {
        return agents.at(step_type);
    }

    // Fallback to primary agent
    return primary_agent();
}

// Execute the planning flow with agents.
std::string PlanningFlow::execute(const std::string& input) {
    try {
        if (agents.find(primary_agent_key) == agents.end()) {
            throw std::runtime_error("No primary agent available");
        }

        // Create initial plan if input provided
        if (!input.empty()) {
            _create_initial_plan(input);

            // Verify plan was created successfully
            if (planning_tool->plans.find(active_plan_id) == planning_tool->plans.end()) {
                logger->error("Plan creation failed. Plan ID " + active_plan_id + " not found in planning tool.");
                return "Failed to create plan for: " + input;
            }
        }

        std::string result = "";
        while (true) {
            // Get current step to execute
            json step_info;
            _get_current_step_info(current_step_index, step_info);

            // Exit if no more steps or plan completed
            if (current_step_index < 0) {
                break;
            }

            // Execute current step with appropriate agent
            std::string step_type = step_info.value("type", "");
            auto executor = get_executor(step_type);
            std::string step_result = _execute_step(executor, step_info);
            // result += step_result + "\n";

            // Check if agent wants to terminate
            if (executor->state == AgentState::FINISHED || executor->state == AgentState::ERR) {
                break;
            }

            // Refactor memory
            std::string prefix_sum = _summarize_plan(executor->memory->get_messages(step_result));
            executor->reset(false);
            executor->update_memory("assistant", prefix_sum);
            if (!input.empty()) {
                executor->update_memory("user", "Continue to accomplish the task: " + input);
            }

            result += "##" + step_info.value("type", "Step " + std::to_string(current_step_index)) + ":\n" + prefix_sum + "\n\n";
        }

        reset(true); // Clear short-term memory and state for next plan

        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("Error executing planning flow: " + std::string(e.what()));
        return "Execution failed: " + std::string(e.what());
    }
}

// Create an initial plan based on the request using the flow's LLM and PlanningTool.
void PlanningFlow::_create_initial_plan(const std::string& request) {
    logger->info("Creating initial plan with ID: " + active_plan_id);

    // Create a system message for plan creation
    std::string system_prompt = "You are a planning assistant. Your task is to create a detailed plan with clear steps.";

    // Create a user message with the request
    std::string user_prompt = "Please provide a detailed plan to accomplish this task: " + request + "\n\n";
    user_prompt += "**Note**: The following executors will be used to accomplish the plan.\n\n";
    for (const auto& [key, agent] : agents) {
        auto tool_call_agent = std::dynamic_pointer_cast<ToolCallAgent>(agent);
        if (tool_call_agent) {
            user_prompt += "Available tools for executor `" + key + "`:\n";
            user_prompt += tool_call_agent->available_tools.to_params().dump(2) + "\n\n";
        }
    }

    // Call LLM with PlanningTool
    auto response = llm->ask_tool(
        {Message::user_message(user_prompt)},
        system_prompt,
        "", // No next_step_prompt for initial plan creation
        json::array({planning_tool->to_param()}),
        "required"
    );

    // Process tool calls if present
    if (response.contains("tool_calls") && !response["tool_calls"].empty()) {
        auto tool_calls = ToolCall::from_json_list(response["tool_calls"]);

        for (const auto& tool_call : tool_calls) {
            // Parse the arguments
            auto args = tool_call.function.arguments;
            if (args.is_string()) {
                try {
                    std::string args_str = args.get<std::string>();
                    args = json::parse(args_str);
                } catch (...) {
                    logger->error("Failed to parse tool arguments: " + args.dump());
                    continue;
                }
            }

            // Ensure plan_id is set correctly and execute the tool
            args["plan_id"] = active_plan_id;

            // Execute the tool via ToolCollection instead of directly
            auto result = planning_tool->execute(args);

            logger->info("Plan creation result: " + result.to_string());
            return;
        }
    }

    // If execution reached here, create a default plan
    logger->warn("Creating default plan");

    // Create default plan using the ToolCollection
    auto title = request;
    if (title.size() > 50) {
        title = title.substr(0, validate_utf8(title.substr(0, 50))) + "...";
    }

    planning_tool->execute({
        {"command", "create"},
        {"plan_id", active_plan_id},
        {"title", title},
        {"steps", {"Analyze request", "Execute task", "Verify results"}}
    });
}

// Parse the current plan to identify the first non-completed step's index and info.
// Returns (-1, None) if no active step is found.
void PlanningFlow::_get_current_step_info(int& current_step_index, json& step_info) {
    if (active_plan_id.empty() || planning_tool->plans.find(active_plan_id) == planning_tool->plans.end()) {
        logger->error("Plan with ID " + active_plan_id + " not found");
        current_step_index = -1;
        step_info = json::object();
        return;
    }

    try {
        // Direct access to plan data from planning tool storage
        const json& plan_data = planning_tool->plans[active_plan_id];
        json steps = plan_data.value("steps", json::array());
        json step_statuses = plan_data.value("step_statuses", json::array());

        // Find first non-completed step
        for (size_t i = 0; i < steps.size(); ++i) {
            const auto& step = steps[i].get<std::string>();
            std::string step_status;

            if (i >= step_statuses.size()) {
                step_status = "not_started";
            } else {
                step_status = step_statuses[i].get<std::string>();
            }
            
            if (step_status == "not_started" || step_status == "in_progress") {
                // Extract step type/category if available
                step_info = {
                    {"type", step}
                };
            } else { // completed or skipped
                continue;
            }

            // Try to extract step type from the text (e.g., [SEARCH] or [CODE])
            std::regex step_regex("\\[([A-Z_]+)\\]");
            std::smatch match;
            if (std::regex_search(step, match, step_regex)) {
                step_info["type"] = match[1].str(); // to_lower?
            }

            // Mark current step as in_progress
            try {
                ToolResult result = planning_tool->execute({
                    {"command", "mark_step"},
                    {"plan_id", active_plan_id},
                    {"step_index", i},
                    {"step_status", "in_progress"}
                });
                logger->info(
                    "Started executing step " + std::to_string(i) + " in plan " + active_plan_id
                    + "\n\n" + result.to_string() + "\n\n"
                );
            } catch (const std::exception& e) {
                logger->error("Error marking step as in_progress: " + std::string(e.what()));
                // Update step status directly if needed
                if (i < step_statuses.size()) {
                    step_statuses[i] = "in_progress";
                } else {
                    while (i > step_statuses.size()) {
                        step_statuses.push_back("not_started");
                    }
                    step_statuses.push_back("in_progress");
                }

                planning_tool->plans[active_plan_id]["step_statuses"] = step_statuses;
            }

            current_step_index = i;
            return;
        }
        current_step_index = -1;
        step_info = json::object(); // No active step found
    } catch (const std::exception& e) {
        logger->error("Error finding current step index: " + std::string(e.what()));
        current_step_index = -1;
        step_info = json::object();
    }
}

// Execute the current step with the specified agent using agent.run().
std::string PlanningFlow::_execute_step(const std::shared_ptr<BaseAgent>& executor, const json& step_info) {
    // Prepare context for the agent with current plan status
    json plan_status = _get_plan_text();
    std::string step_text = step_info.value("text", "Step " + std::to_string(current_step_index));

    // Create a prompt for the agent to execute the current step
    std::string step_prompt;
    step_prompt += "\nCURRENT PLAN STATUS:\n";
    step_prompt += plan_status.dump(2);
    step_prompt += "\n\nYOUR CURRENT TASK:\n";
    step_prompt += "You are now working on step " + std::to_string(current_step_index) + ": \"" + step_text + "\"\n";
    step_prompt += "Please execute this step using the appropriate tools. When you're done, provide a summary of what you accomplished and call `terminate` to trigger the next step.";

    // Use agent.run() to execute the step
    try {
        std::string step_result = executor->run(step_prompt);

        // Mark the step as completed after successful execution
        if (executor->state != AgentState::ERR) {
            _mark_step_completed();
        }

        return step_result;
    } catch (const std::exception& e) {
        LOG_ERROR("Error executing step " + std::to_string(current_step_index) + ": " + std::string(e.what()));
        return "Error executing step " + std::to_string(current_step_index) + ": " + std::string(e.what());
    }
}

// Mark the current step as completed.
void PlanningFlow::_mark_step_completed() {
    if (current_step_index < 0) {
        return;
    }

    try {
        // Mark the step as completed
        ToolResult result = planning_tool->execute({
            {"command", "mark_step"},
            {"plan_id", active_plan_id},
            {"step_index", current_step_index},
            {"step_status", "completed"}
        });
        logger->info(
            "Marked step " + std::to_string(current_step_index) + " as completed in plan " + active_plan_id
            + "\n\n" + result.to_string() + "\n\n"
        );
    } catch (const std::exception& e) {
        logger->warn("Failed to update plan status: " + std::string(e.what()));
        // Update step status directly in planning tool storage
        if (planning_tool->plans.find(active_plan_id) != planning_tool->plans.end()) {
            const json& plan_data = planning_tool->plans[active_plan_id];
            json step_statuses = plan_data.value("step_statuses", json::array());

            // Ensure the step_statuses list is long enough
            while (current_step_index >= step_statuses.size()) {
                step_statuses.push_back("not_started");
            }

            // Update the status
            step_statuses[current_step_index] = "completed";
            planning_tool->plans[active_plan_id]["step_statuses"] = step_statuses;
        }
    }
}

// Get the current plan as formatted text.
std::string PlanningFlow::_get_plan_text() {
    try {
        auto result = planning_tool->execute({
            {"command", "get"},
            {"plan_id", active_plan_id}
        });

        return result.to_string();
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting plan: " + std::string(e.what()));
        return _generate_plan_text_from_storage();
    }
}

// Generate plan text directly from storage if the planning tool fails.
std::string PlanningFlow::_generate_plan_text_from_storage() {
    try {
        if (planning_tool->plans.find(active_plan_id) == planning_tool->plans.end()) {
            return "Error: Plan with ID " + active_plan_id + " not found";
        }

        const json& plan_data = planning_tool->plans[active_plan_id];
        auto title = plan_data.value("title", "Untitled Plan");
        auto steps = plan_data.value("steps", json::array());
        auto step_statuses = plan_data.value("step_statuses", json::array());
        auto step_notes = plan_data.value("step_notes", json::array());

        // Ensure step_statuses and step_notes match the number of steps
        while (step_statuses.size() < steps.size()) {
            step_statuses.push_back("not_started");
        }
        while (step_notes.size() < steps.size()) {
            step_notes.push_back("");
        }
        
        // Count steps by status
        std::map<std::string, int> status_counts = {
            {"completed", 0},
            {"in_progress", 0},
            {"blocked", 0},
            {"not_started", 0}
        };

        for (const auto& status : step_statuses) {
            if (status_counts.find(status) != status_counts.end()) { 
                status_counts[status] = status_counts[status] + 1;
            }
        }

        int completed = status_counts["completed"];
        int total = steps.size();
        double progress = total > 0 ? (static_cast<double>(completed) / total) * 100.0 : 0.0;

        std::stringstream plan_text_ss;
        plan_text_ss << "Plan: " << title << "(ID: " << active_plan_id << ")\n";
        plan_text_ss << std::string(plan_text_ss.str().size(), '=') << "\n\n";

        plan_text_ss << "Total steps: " << completed << "/" << total << " steps completed (" << std::fixed << std::setprecision(1) << progress << "%)\n";
        plan_text_ss << "Status: " << status_counts["completed"] << " completed, " << status_counts["in_progress"] << " in progress, " 
                     << status_counts["blocked"] << " blocked, " << status_counts["not_started"] << " not started\n\n";
        plan_text_ss << "Steps:\n";

        for (size_t i = 0; i < steps.size(); ++i) {
            auto step = steps[i];
            auto status = step_statuses[i];
            auto notes = step_notes[i];

            std::string status_mark;

            if (status == "completed") {
                status_mark = "[✓]"; 
            } else if (status == "in_progress") {
                status_mark = "[→]";
            } else if (status == "blocked") {
                status_mark = "[!]";
            } else if (status == "not_started") {
                status_mark = "[ ]";
            } else { // unknown status
                status_mark = "[?]";
            }

            plan_text_ss << i << ". " << status_mark << " " << step << "\n";
            if (!notes.empty()) {
                plan_text_ss << "    Notes: " << notes << "\n";
            }
        }

        return plan_text_ss.str();
    } catch (const std::exception& e) {
        logger->error("Error generating plan text from storage: " + std::string(e.what()));
        return "Error: Unable to retrieve plan with ID " + active_plan_id;
    }
}

// Summarize the plan using the flow's LLM directly
std::string PlanningFlow::_summarize_plan(const std::vector<Message> messages) {
    std::string plan_text = _get_plan_text();

    std::string system_prompt = "You are a planning assistant. Your task is to summarize the current plan.";
    std::string next_step_prompt = "Above is the nearest finished step in the plan. Here is the current plan status:\n\n" + plan_text + "\n\n"
                                + "Please provide a summary of what was accomplished and any thoughts for next steps (when the plan is not fully finished).";

    // Create a summary using the flow's LLM directly
    try {
        auto response = llm->ask(
            messages,
            system_prompt,
            next_step_prompt
        );

        return response;
    } catch (const std::exception& e) {
        LOG_ERROR("Error summarizing plan with LLM: " + std::string(e.what()));
        // Fallback to using an agent for the summary
        try {
            auto agent = primary_agent();
            std::string summary = agent->run(system_prompt + next_step_prompt);
            return summary;
        } catch (const std::exception& e2) {
            LOG_ERROR("Error summarizing plan with agent: " + std::string(e2.what()));
            return "Error generating summary.";
        }
    }
}

}