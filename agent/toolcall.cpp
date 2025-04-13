#include "toolcall.h"

namespace humanus {

// Process current state and decide next actions using tools
bool ToolCallAgent::think() {
    // Get response with tool options
    auto response = llm->ask_tool(
        memory->get_messages(memory->current_request),
        system_prompt,
        next_step_prompt,
        available_tools.to_params(),
        tool_choice
    );

    tool_calls = ToolCall::from_json_list(response["tool_calls"]);

    // Log response info
    logger->info("✨ " + name + "'s thoughts: " + (response["content"].empty() ? "<no content>" : response["content"].get<std::string>()));
    logger->info(
        "🛠️  " + name + " selected " + std::to_string(tool_calls.size()) + " tool(s) to use"
    );
    if (tool_calls.size() > 0) {
        std::string tools_str;
        for (const auto& tool_call : tool_calls) {
            tools_str += tool_call.function.name + " ";
        }
        logger->info(
            "🧰 Tools being prepared: " + tools_str
        );
    }

    if (state != AgentState::RUNNING) {
        return false;
    }

    try {
        // Handle different tool_choices modes
        if (tool_choice == "none") {
            if (tool_calls.size() > 0) {
                logger->warn("🤔 Hmm, " + name + "tried to use tools when they weren't available!");
            }
            if (!response["content"].empty()) {
                memory->add_message(Message::assistant_message(response["content"]));
                return true;
            }
            return false;
        } 

        // Create and add assistant message
        auto assistant_msg = Message::assistant_message(response["content"], tool_calls);
        memory->add_message(assistant_msg);

        if (tool_choice == "required" && tool_calls.empty()) {
            return true; // Will be handled in act()
        }

        return !tool_calls.empty();
    } catch (const std::exception& e) {
        logger->error("🚨 Oops! The " + name + "'s thinking process hit a snag: " + std::string(e.what()));
        return false;
    }
}

// Execute tool calls and handle their results
std::string ToolCallAgent::act() {
    if (tool_calls.empty()) {
        if (tool_choice == "required") {
            throw std::runtime_error("Required tools but none selected");
        }

        // Return last message content if no tool calls
        return memory->get_messages().empty() || memory->get_messages().back().content.empty() ? "No content or commands to execute" : memory->get_messages().back().content.dump();
    }

    std::vector<ToolResult> results;

    std::string result_str;

    for (const auto& tool_call : tool_calls) {
        auto result = state == AgentState::RUNNING ? 
                    execute_tool(tool_call) : 
                    ToolError("Agent is not running, so no more tool calls will be executed.");

        logger->info(
            "🎯 Tool `" + tool_call.function.name + "` completed its mission! Result: " + result.to_string(500)
        );

        if (result.to_string().size() > 12288) { // Pre-check before tokenization (will be done in Message constructor)
            if (!(result.output.size() == 1 && result.output[0]["type"] == "image_url")) { 
                // If the result is not an image, split the result into multiple chunks and save to memory
                // Might be long text or mixture of text and image
                result = content_provider->handle_write({
                    {"content", result.output}
                });
                logger->info("🔍 Tool result for `" + tool_call.function.name + "` has been split into multiple chunks and saved to memory.");
                result.output = "This tool call has been split into multiple chunks and saved to memory. Please refer to below information to use the `content_provider` tool to read the chunks:\n" + result.to_string();
            }
        }

        // Add tool response to memory
        Message tool_msg = Message::tool_message(
            result.error.empty() ? result.output : result.error, tool_call.id, tool_call.function.name
        );

        // If the tool message is too long, use the `content_provider` tool to split the message into multiple chunks
        if (tool_msg.num_tokens > 4096) { // TODO: Make this configurable)
            auto result = content_provider->handle_write({
                {"content", tool_msg.content}
            });
            logger->info("🔍 Tool result for `" + tool_call.function.name + "` has been split into multiple chunks and saved to memory.");
            tool_msg = Message::tool_message(
                "This tool call has been split into multiple chunks and saved to memory. Please refer to below information to use the `content_provider` tool to read the chunks:\n" + result.to_string(),
                tool_call.id,
                tool_call.function.name
            );
        }

        memory->add_message(tool_msg);
        
        auto observation = result.empty() ?
                            "Tool `" + tool_msg.name + "` completed with no output" :
                            "Observed output of tool `" + tool_msg.name + "` executed:\n" + result.to_string();

        result_str += observation + "\n\n";
    }

    return result_str;
}

ToolResult ToolCallAgent::execute_tool(ToolCall tool_call) {
    if (tool_call.empty() || tool_call.function.empty() || tool_call.function.name.empty()) {
        return ToolError("Invalid command format");
    }

    std::string name = tool_call.function.name;
    if (available_tools.tools_map.find(name) == available_tools.tools_map.end()) {
        return ToolError("Unknown tool `" + name + "`. Please use one of the following tools: " + 
               std::accumulate(available_tools.tools_map.begin(), available_tools.tools_map.end(), std::string(),
                               [](const std::string& a, const auto& b) {
                                   return a + (a.empty() ? "" : ", ") + b.first;
                               })
        );
    }

    try {
        // Parse arguments
        json args = tool_call.function.arguments;

        if (args.is_string()) {
            args = json::parse(args.get<std::string>());
        }

        // Execute the tool
        logger->info("🔧 Activating tool: `" + name + "`...");
        ToolResult result = available_tools.execute(name, args);

        // Handle special tools like `finish`
        _handle_special_tool(name, result);

        return result;
    } catch (const json::exception& /* e */) {
        std::string error_msg = "Error parsing arguments for " + name + ": Invalid JSON format";
        logger->error(
            "📝 Oops! The arguments for `" + name + "` don't make sense - invalid JSON"
        );
        return ToolError(error_msg);
    } catch (const std::exception& e) {
        std::string error_msg = "⚠️ Tool `" + name + "` encountered a problem: " + std::string(e.what());
        logger->error(error_msg);
        return ToolError(error_msg);
    }
}

// Handle special tool execution and state changes
void ToolCallAgent::_handle_special_tool(const std::string& name, const ToolResult& result, const json& kwargs) {
    if (!_is_special_tool(name)) {
        return;
    }

    if (_should_finish_execution(name, result, kwargs)) {
        logger->info("🏁 Special tool `" + name + "` has completed the task!");
        state = AgentState::FINISHED;
    }
}

// Determine if tool execution should finish the agent
bool ToolCallAgent::_should_finish_execution(const std::string& name, const ToolResult& result, const json& kwargs) {
    return true; // Currently, all special tools (terminate) finish the agent
}

bool ToolCallAgent::_is_special_tool(const std::string& name) {
    return special_tool_names.find(name) != special_tool_names.end();
}
    
}