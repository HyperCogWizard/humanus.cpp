#ifndef HUMANUS_TOOL_COLLECTION_H
#define HUMANUS_TOOL_COLLECTION_H

#include "base.h"
#include "image_loader.h"
#include "python_execute.h"
#include "filesystem.h"
#include "playwright.h"
#include "puppeteer.h"
#include "content_provider.h"
#include "terminate.h"

namespace humanus {

struct ToolCollection {
    std::vector<std::shared_ptr<BaseTool>> tools;
    std::map<std::string, std::shared_ptr<BaseTool>> tools_map;

    ToolCollection() = default;

    ToolCollection(std::vector<std::shared_ptr<BaseTool>> tools) : tools(tools) {
        for (auto tool : tools) {
            tools_map[tool->name] = tool;
        }
    }

    json to_params() const {
        json params = json::array();
        for (auto tool : tools) {
            params.push_back(tool->to_param());
        }
        return params;
    }
    
    ToolResult execute(const std::string& name, const json& args) const {
        auto tool_iter = tools_map.find(name);
        if (tool_iter == tools_map.end()) {
            return ToolError("Tool `" + name + "` not found");
        }
        try {
            return tool_iter->second->execute(args);
        } catch (const std::exception& e) {
            return ToolError(e.what());
        }
    }

    // Execute all tools in the collection sequentially.
    std::vector<ToolResult> execute_all(const json& args) const { // No reference now
        std::vector<ToolResult> results;
        for (auto tool : tools) {
            try {
                auto result = tool->execute(args);
                results.push_back(result);
            } catch (const std::exception& e) {
                results.push_back(ToolError(e.what()));
            }
        }
        return results;
    }

    void add_tool(const std::shared_ptr<BaseTool>& tool) {
        tools.push_back(tool);
        tools_map[tool->name] = tool;
    }

    void add_mcp_tools(const std::string& mcp_server_name) {
        auto client = BaseMCPTool::create_client(mcp_server_name);
        auto tool_list = client->get_tools();
        for (auto tool : tool_list) {
            add_tool(std::make_shared<BaseMCPTool>(tool.name, tool.description, tool.parameters_schema, client));
        }
    }

    void add_tools(const std::vector<std::shared_ptr<BaseTool>>& tools) {
        for (auto tool : tools) {
            add_tool(tool);
        }
    }

    std::shared_ptr<BaseTool> get_tool(const std::string& name) const {
        auto tool_iter = tools_map.find(name);
        if (tool_iter == tools_map.end()) {
            return nullptr;
        }
        return tool_iter->second;
    }
    
    std::vector<std::string> get_tool_names() const {
        std::vector<std::string> names;
        for (const auto& tool : tools) {
            names.push_back(tool->name);
        }
        return names;
    }
};

class ToolFactory {
public:
    static std::shared_ptr<BaseTool> create(const std::string& name) {
        if (name == "python_execute") {
            return std::make_shared<PythonExecute>();
        } else if (name == "filesystem") {
            return std::make_shared<Filesystem>();
        } else if (name == "playwright") {
            return std::make_shared<Playwright>();
        } else if (name == "puppeteer") {
            return std::make_shared<Puppeteer>();
        } else if (name == "image_loader") {
            return std::make_shared<ImageLoader>();
        } else if (name == "content_provider") {
            return std::make_shared<ContentProvider>();
        } else if (name == "terminate") {
            return std::make_shared<Terminate>();
        } else {
            return nullptr;
        }
    }
};

} // namespace humanus

#endif // HUMANUS_TOOL_COLLECTION_H