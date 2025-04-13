#ifndef HUMANUS_AGENT_TOOLCALL_H
#define HUMANUS_AGENT_TOOLCALL_H

#include "react.h"
#include "prompt.h"
#include "tool/tool_collection.h"
#include "tool/terminate.h"
#include "tool/content_provider.h"

namespace humanus {

// Base agent class for handling tool/function calls with enhanced abstraction
struct ToolCallAgent : ReActAgent {
    std::vector<ToolCall> tool_calls;
    ToolCollection available_tools;
    std::string tool_choice;
    std::set<std::string> special_tool_names;

    std::shared_ptr<ContentProvider> content_provider;

    ToolCallAgent(
        const ToolCollection& available_tools = ToolCollection(
            {
                std::make_shared<ContentProvider>(),
                std::make_shared<Terminate>()
            }
        ),
        const std::string& tool_choice = "auto",
        const std::set<std::string>& special_tool_names = {"terminate"},
        const std::string& name = "toolcall", 
        const std::string& description = "an agent that can execute tool calls.", 
        const std::string& system_prompt = prompt::toolcall::SYSTEM_PROMPT,
        const std::string& next_step_prompt = prompt::toolcall::NEXT_STEP_PROMPT,
        const std::shared_ptr<LLM>& llm = nullptr,
        const std::shared_ptr<BaseMemory>& memory = nullptr,
        int max_steps = 30,
        int duplicate_threshold = 2
    ) : ReActAgent(
            name, 
            description, 
            system_prompt, 
            next_step_prompt, 
            llm, 
            memory, 
            max_steps, 
            duplicate_threshold
        ),
        available_tools(available_tools),
        tool_choice(tool_choice),
        special_tool_names(special_tool_names) {
        if (this->available_tools.tools_map.find("terminate") == this->available_tools.tools_map.end()) {
            this->available_tools.add_tool(std::make_shared<Terminate>());
        }
        if (this->available_tools.tools_map.find("content_provider") == this->available_tools.tools_map.end()) {
            content_provider = std::make_shared<ContentProvider>();
            this->available_tools.add_tool(content_provider);
        } else {
            content_provider = std::dynamic_pointer_cast<ContentProvider>(this->available_tools.tools_map["content_provider"]);
        }
    }

    // Process current state and decide next actions using tools
    bool think() override;

    // Execute tool calls and handle their results
    std::string act() override;
    
    // Execute a single tool call with robust error handling
    ToolResult execute_tool(ToolCall tool_call);

    // Handle special tool execution and state changes
    void _handle_special_tool(const std::string& name, const ToolResult& result, const json& kwargs = {});

    // Determine if tool execution should finish the agent
    static bool _should_finish_execution(const std::string& name, const ToolResult& result, const json& kwargs = {});

    bool _is_special_tool(const std::string& name);
    
};

}

#endif // HUMANUS_AGENT_TOOLCALL_H
