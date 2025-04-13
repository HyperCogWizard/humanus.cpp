#include "humanus.h"

namespace humanus {

std::string Humanus::run(const std::string& request) {
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

Humanus Humanus::load_from_toml(const toml::table& config_table) {
    try {
        // Tools
        std::vector<std::string> tools;
        std::vector<std::string> mcp_servers;

        if (config_table.contains("tools")) {
            auto tools_table = *config_table["tools"].as_array();
            for (const auto& tool : tools_table) {
                tools.push_back(tool.as_string()->get());
            }
        } else {
            tools = {"python_execute", "filesystem", "playwright", "image_loader", "content_provider", "terminate"};
        }

        if (config_table.contains("mcp_servers")) {
            auto mcp_servers_table = *config_table["mcp_servers"].as_array();
            for (const auto& server : mcp_servers_table) {
                mcp_servers.push_back(server.as_string()->get());
            }
        }

        ToolCollection available_tools;

        for (const auto& tool : tools) {
            auto tool_ptr = ToolFactory::create(tool);
            if (tool_ptr) {
                available_tools.add_tool(tool_ptr);
            } else {
                logger->warn("Tool `" + tool + "` not found in tool registry, skipping...");
            }
        }

        for (const auto& mcp_server : mcp_servers) {
            available_tools.add_mcp_tools(mcp_server);
        }
        
        // General settings
        std::string name, description, system_prompt, next_step_prompt;
        if (config_table.contains("name")) {
            name = config_table["name"].as_string()->get();
        } else {
            name = "humanus";
        }
        if (config_table.contains("description")) {
            description = config_table["description"].as_string()->get();
        } else {
            description = "A versatile agent that can solve various tasks using multiple tools";
        }
        if (config_table.contains("system_prompt")) {
            system_prompt = config_table["system_prompt"].as_string()->get();
        } else {
            system_prompt = prompt::humanus::SYSTEM_PROMPT;
        }
        if (config_table.contains("next_step_prompt")) {
            next_step_prompt = config_table["next_step_prompt"].as_string()->get();
        } else {
            next_step_prompt = prompt::humanus::NEXT_STEP_PROMPT;
        }
        
        // Workflow settings
        std::shared_ptr<LLM> llm = nullptr;
        if (config_table.contains("llm")) {
            llm = LLM::get_instance(config_table["llm"].as_string()->get());
        }
        
        std::shared_ptr<Memory> memory = nullptr;
        if (config_table.contains("memory")) {
            memory = std::make_shared<Memory>(Config::get_memory_config(config_table["memory"].as_string()->get()));
        }

        int max_steps = 30;
        if (config_table.contains("max_steps")) {
            max_steps = config_table["max_steps"].as_integer()->get();
        }

        int duplicate_threshold = 2;
        if (config_table.contains("duplicate_threshold")) {
            duplicate_threshold = config_table["duplicate_threshold"].as_integer()->get();
        }

        return Humanus(
            available_tools,
            name,
            description,
            system_prompt,
            next_step_prompt,
            llm,
            memory,
            max_steps,
            duplicate_threshold
        );
    } catch (const std::exception& e) {
        logger->error("Error loading Humanus from TOML: " + std::string(e.what()));
        throw;
    }
}

Humanus Humanus::load_from_json(const json& config_json) {
    try {
        // Tools
        std::vector<std::string> tools;
        std::vector<std::string> mcp_servers;

        if (config_json.contains("tools")) {
            tools = config_json["tools"].get<std::vector<std::string>>();
        }

        if (config_json.contains("mcp_servers")) {
            mcp_servers = config_json["mcp_servers"].get<std::vector<std::string>>();
        }

        ToolCollection available_tools;

        for (const auto& tool : tools) {
            auto tool_ptr = ToolFactory::create(tool);
            if (tool_ptr) {
                available_tools.add_tool(tool_ptr);
            }
        }

        for (const auto& mcp_server : mcp_servers) {
            available_tools.add_mcp_tools(mcp_server);
        }

        // General settings
        std::string name, description, system_prompt, next_step_prompt;
        if (config_json.contains("name")) {
            name = config_json["name"].get<std::string>();
        } else {
            name = "humanus";
        }

        if (config_json.contains("description")) {
            description = config_json["description"].get<std::string>();
        } else {
            description = "A versatile agent that can solve various tasks using multiple tools";
        }

        if (config_json.contains("system_prompt")) {
            system_prompt = config_json["system_prompt"].get<std::string>();
        } else {
            system_prompt = prompt::humanus::SYSTEM_PROMPT;
        }
        
        if (config_json.contains("next_step_prompt")) {
            next_step_prompt = config_json["next_step_prompt"].get<std::string>();
        } else {
            next_step_prompt = prompt::humanus::NEXT_STEP_PROMPT;
        }

        // Workflow settings
        std::shared_ptr<LLM> llm = nullptr;
        if (config_json.contains("llm")) {
            llm = LLM::get_instance(config_json["llm"].get<std::string>());
        }
        
        std::shared_ptr<Memory> memory = nullptr;
        if (config_json.contains("memory")) {
            memory = std::make_shared<Memory>(Config::get_memory_config(config_json["memory"].get<std::string>()));
        }

        int max_steps = 30;
        if (config_json.contains("max_steps")) {
            max_steps = config_json["max_steps"].get<int>();
        }

        int duplicate_threshold = 2;
        if (config_json.contains("duplicate_threshold")) {
            duplicate_threshold = config_json["duplicate_threshold"].get<int>();
        }

        return Humanus(
            available_tools,
            name,
            description,
            system_prompt,
            next_step_prompt,
            llm,
            memory,
            max_steps,
            duplicate_threshold
        );
    } catch (const std::exception& e) {
        logger->error("Error loading Humanus from JSON: " + std::string(e.what()));
        throw;
    }
}

}