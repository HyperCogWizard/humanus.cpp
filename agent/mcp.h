#ifndef HUMANUS_AGENT_MCP_H
#define HUMANUS_AGENT_MCP_H

#include "base.h"
#include "toolcall.h"
#include "prompt.h"

namespace humanus {

struct MCPAgent : ToolCallAgent {
    MCPAgent(
        const std::vector<std::string>& mcp_servers,
        const ToolCollection& available_tools = ToolCollection(
            {
                std::make_shared<Terminate>()
            }
        ),
        const std::string& tool_choice = "auto",
        const std::set<std::string>& special_tool_names = {"terminate"},
        const std::string& name = "mcp_agent", 
        const std::string& description = "an agent that can execute tool calls.", 
        const std::string& system_prompt = prompt::toolcall::SYSTEM_PROMPT,
        const std::string& next_step_prompt = prompt::toolcall::NEXT_STEP_PROMPT,
        const std::shared_ptr<LLM>& llm = nullptr,
        const std::shared_ptr<BaseMemory>& memory = nullptr,
        int max_steps = 30,
        int duplicate_threshold = 2 
    ) : ToolCallAgent(
            available_tools,
            tool_choice,
            special_tool_names,
            name,
            description,
            system_prompt,
            next_step_prompt,
            llm,
            memory,
            max_steps,
            duplicate_threshold
        ) {
        for (const auto& server_name : mcp_servers) {
            this->available_tools.add_mcp_tools(server_name);
        }
    }

    std::string run(const std::string& request = "") override {
        memory->current_request = request;
        
        auto tmp_next_step_prompt = next_step_prompt;
    
        size_t pos = next_step_prompt.find("{current_date}");
        if (pos != std::string::npos) {
            // %Y-%d-%m
            auto current_date = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(current_date);
            std::stringstream ss;
            std::tm tm_info = *std::localtime(&in_time_t);
            ss << std::put_time(&tm_info, "%Y-%m-%d");
            std::string formatted_date = ss.str(); // YYYY-MM-DD
            next_step_prompt.replace(pos, 14, formatted_date);
        }
    
        pos = next_step_prompt.find("{current_request}");
        if (pos != std::string::npos) {
            next_step_prompt.replace(pos, 17, request);
        }
        
        auto result = BaseAgent::run(request);
        next_step_prompt = tmp_next_step_prompt; // restore the original prompt
    
        return result;
    }
};

}

#endif // HUMANUS_AGENT_MCP_H