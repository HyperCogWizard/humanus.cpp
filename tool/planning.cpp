#include "planning.h"
#include <iomanip>

namespace humanus {

/**
* Execute the planning tool with the given command and parameters.
*
* Parameters:
* - command: The operation to perform
* - plan_id: Unique identifier for the plan
* - title: Title for the plan (used with create command)
* - steps: List of steps for the plan (used with create command)
* - step_index: Index of the step to update (used with mark_step command)
* - step_status: Status to set for a step (used with mark_step command)
* - step_notes: Additional notes for a step (used with mark_step command)
*/
ToolResult PlanningTool::execute(const json& args) {
    try {
        std::string command = args.value("command", "");
        std::string plan_id = args.value("plan_id", "");
        std::string title = args.value("title", "");
        std::vector<std::string> steps = args.value("steps", std::vector<std::string>());
        int step_index = args.value("step_index", -1);
        std::string step_status = args.value("step_status", "not_started");
        std::string step_notes = args.value("step_notes", "");
        
        if (command == "create") {
            return _create_plan(plan_id, title, steps);
        } else if (command == "update") {
            return _update_plan(plan_id, title, steps);
        } else if (command == "list") {
            return _list_plans();
        } else if (command == "get") {
            return _get_plan(plan_id);
        } else if (command == "set_active") {
            return _set_active_plan(plan_id);
        } else if (command == "mark_step") {
            return _mark_step(plan_id, step_index, step_status, step_notes);
        } else if (command == "delete") {
            return _delete_plan(plan_id);
        } else {
            throw std::runtime_error("Unrecognized command: " + command + ". Allowed commands are: create, update, list, get, set_active, mark_step, delete");
        }
    } catch (const std::exception& e) {
        return ToolError(e.what());
    }
}

// Create a new plan with the given ID, title, and steps.
ToolResult PlanningTool::_create_plan(const std::string& plan_id, const std::string& title, const std::vector<std::string>& steps) {
    if (plan_id.empty()) {
        return ToolError("Parameter `plan_id` is required for command: create");
    }

    if (plans.find(plan_id) != plans.end()) {
        return ToolError("Plan with ID " + plan_id + " already exists. Use 'update' to modify existing plans.");
    }

    if (title.empty()) {
        return ToolError("Parameter `title` is required for command: create");
    }

    if (steps.empty()) {
        return ToolError("Parameter `steps` must be a non-empty list of strings for command: create");
    }
    
    // Create a new plan with initialized step statuses
    json plan = {
        {"plan_id", plan_id},
        {"title", title},
        {"steps", steps},
        {"step_statuses", std::vector<std::string>(steps.size(), "not_started")},
        {"step_notes", std::vector<std::string>(steps.size(), "")}
    };

    plans[plan_id] = plan;
    _current_plan_id = plan_id; // Set as active plan

    return ToolResult(
        "Plan created successfully with ID: " + plan_id + "\n\n" + _format_plan(plan)
    );
}

// Update an existing plan with new title or steps.
ToolResult PlanningTool::_update_plan(const std::string& plan_id, const std::string& title, const std::vector<std::string>& steps) {
    if (plan_id.empty()) {
        return ToolError("Parameter `plan_id` is required for command: update");
    }

    if (plans.find(plan_id) == plans.end()) {
        return ToolError("No plan found with ID: " + plan_id);
    }
    
    json plan = plans[plan_id];

    if (!title.empty()) {
        plan["title"] = title;
    }

    if (!steps.empty()) {
        // Preserve existing step statuses for unchanged steps
        auto old_steps = plan["steps"];
        auto old_step_statuses = plan["step_statuses"];
        auto old_step_notes = plan["step_notes"];
 
        // Create new step statuses and notes
        std::vector<std::string> new_step_statuses;
        std::vector<std::string> new_step_notes;

        for (size_t i = 0; i < steps.size(); ++i) {
            // If the step exists at the same position in old steps, preserve status and notes
            if (i < old_steps.size() && old_steps[i] == steps[i]) {
                new_step_statuses.push_back(old_step_statuses[i]);
                new_step_notes.push_back(old_step_notes[i]);
            } else {
                new_step_statuses.push_back("not_started");
                new_step_notes.push_back("");
            }
        }

        plan["steps"] = steps;
        plan["step_statuses"] = new_step_statuses;
        plan["step_notes"] = new_step_notes;
    }

    plans[plan_id] = plan; // Note: Remember to update the plan in the map

    return ToolResult(
        "Plan updated successfully with ID: " + plan_id + "\n\n" + _format_plan(plan)
    );
}

// List all available plans.
ToolResult PlanningTool::_list_plans() {
    if (plans.empty()) {
        return ToolResult(
            "No plans available. Create a plan with the 'create' command."
        );
    }

    std::string output = "Available plans:\n";
    for (const auto& [plan_id, plan] : plans) {
        std::string current_marker = plan_id == _current_plan_id ? " (active)" : "";
        bool completed = std::all_of(plan["step_statuses"].begin(), plan["step_statuses"].end(), [](const std::string& status) {
            return status == "completed";
        });
        int total = plan["steps"].size();
        std::string progress = std::to_string(completed) + "/" + std::to_string(total) + " steps completed";
        output += "• " + plan_id + current_marker + ": " + plan.value("title", "Unknown Plan") + " - " + progress + "\n";
    }

    return ToolResult(output);
}

// Get details of a specific plan.
ToolResult PlanningTool::_get_plan(const std::string& plan_id) {
    std::string _plan_id = plan_id;

    if (plan_id.empty()) {
        // If no plan_id is provided, use the current active plan
        if (_current_plan_id.empty()) {
            return ToolError("No active plan. Please specify a plan_id or set an active plan.");
        }
        _plan_id = _current_plan_id;
    }
    
    if (plans.find(_plan_id) == plans.end()) {
        return ToolError("No plan found with ID: " + _plan_id);
    }

    json plan = plans[_plan_id];
    return ToolResult(_format_plan(plan));
}

// Set a plan as the active plan.
ToolResult PlanningTool::_set_active_plan(const std::string& plan_id) {
    if (plan_id.empty()) {
        return ToolError("Parameter `plan_id` is required for command: set_active");
    }

    if (plans.find(plan_id) == plans.end()) {
        return ToolError("No plan found with ID: " + plan_id);
    }

    _current_plan_id = plan_id;
    return ToolResult(
        "Plan '" + plan_id + "' is now the active plan.\n\n" + _format_plan(plans[plan_id])
    );
}

// Mark a step with a specific status and optional notes.
ToolResult PlanningTool::_mark_step(const std::string& plan_id, int step_index, const std::string& step_status, const std::string& step_notes) {
    std::string _plan_id = plan_id;

    if (plan_id.empty()) {
        // If no plan_id is provided, use the current active plan
        if (_current_plan_id.empty()) {
            return ToolError("No active plan. Please specify a plan_id or set an active plan.");
        }
        _plan_id = _current_plan_id;
    }

    if (plans.find(_plan_id) == plans.end()) {
        return ToolError("No plan found with ID: " + _plan_id);
    }

    json plan = plans[_plan_id];
    
    if (step_index < 0 || step_index >= plan["steps"].size()) {
        return ToolError("Invalid step index: " + std::to_string(step_index) + ". Valid indices range from 0 to " + std::to_string((int)plan["steps"].size() - 1));
    }

    if (!step_status.empty()) {
        if (step_status != "not_started" && step_status != "in_progress" && step_status != "completed" && step_status != "blocked") {
            return ToolError("Invalid step status: " + step_status + ". Valid statuses are: not_started, in_progress, completed, blocked");
        }
        plan["step_statuses"][step_index] = step_status;
    }

    if (!step_notes.empty()) {
        plan["step_notes"][step_index] = step_notes;
    }

    plans[_plan_id] = plan;

    return ToolResult(
        "Step " + std::to_string(step_index) + " updated in plan '" + _plan_id + "'.\n\n" + _format_plan(plan)
    );
}

// Delete a plan.
ToolResult PlanningTool::_delete_plan(const std::string& plan_id) {
    if (plan_id.empty()) {
        return ToolError("Parameter `plan_id` is required for command: delete");
    }

    if (plans.find(plan_id) == plans.end()) {
        return ToolError("No plan found with ID: " + plan_id);
    }

    plans.erase(plan_id);
    
    // If the deleted plan was the active plan, clear the active plan
    if (_current_plan_id == plan_id) {
        _current_plan_id.clear();
    }

    return ToolResult(
        "Plan '" + plan_id + "' has been deleted."
    );
}
    
// Format a plan for display.
std::string PlanningTool::_format_plan(const json& plan) {
    std::stringstream output_ss;
    output_ss << "Plan: " << plan.value("title", "Unknown Plan") << " (ID: " << plan["plan_id"].get<std::string>() << ")\n";
    int current_length = output_ss.str().length();

    for (int i = 0; i < current_length; i++) {
        output_ss << "=";
    }

    output_ss << "\n\n";
    
    // Calculate progress statistics
    int total_steps = plan["steps"].size();
    int completed_steps = std::count_if(plan["step_statuses"].begin(), plan["step_statuses"].end(), [](const std::string& status) {
        return status == "completed";
    });
    int in_progress_steps = std::count_if(plan["step_statuses"].begin(), plan["step_statuses"].end(), [](const std::string& status) {
        return status == "in_progress";
    });
    int blocked_steps = std::count_if(plan["step_statuses"].begin(), plan["step_statuses"].end(), [](const std::string& status) {
        return status == "blocked";
    });
    int not_started_steps = std::count_if(plan["step_statuses"].begin(), plan["step_statuses"].end(), [](const std::string& status) {
        return status == "not_started";
    });

    output_ss << "Progress: " << completed_steps << "/" << total_steps << " steps completed ";
    if (total_steps > 0) {
        double percentage = (double)completed_steps / total_steps * 100;
        output_ss << "(" << std::fixed << std::setprecision(1) << percentage << "%)\n";
    } else {
        output_ss << "(0%)\n";
    }
    
    output_ss << "Status: " << completed_steps << " completed, " << in_progress_steps << " in progress, " << blocked_steps << " blocked, " << not_started_steps << " not started\n\n";
    output_ss << "Steps:\n";

    static std::map<std::string, std::string> status_symbols = {
        {"not_started", "[ ]"},
        {"in_progress", "[→]"},
        {"completed", "[✓]"},
        {"blocked", "[!]"}
    };

    // Add each step with its status and notes
    for (size_t i = 0; i < plan["steps"].size(); ++i) {
        std::string step = plan["steps"][i];
        std::string step_status = plan["step_statuses"][i];
        std::string step_notes = plan["step_notes"][i];
        std::string status_symbol = status_symbols.find(step_status) != status_symbols.end() ? status_symbols[step_status] : "[ ]";
        output_ss << i << ". " + status_symbols[step_status] << " " << step << "\n";
        if (!step_notes.empty()) {
            output_ss << "    Notes: " << step_notes << "\n";
        }
    }

    return output_ss.str();
}

} // namespace humanus