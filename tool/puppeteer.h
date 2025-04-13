#ifndef HUMANUS_TOOL_PUPPETEER_H
#define HUMANUS_TOOL_PUPPETEER_H

#include "base.h"

namespace humanus {

// https://github.com/modelcontextprotocol/servers/tree/HEAD/src/puppeteer
struct Puppeteer : BaseMCPTool {
    inline static const std::string name_ = "puppeteer";
    inline static const std::string description_ = "A Model Context Protocol server that provides browser automation capabilities using Puppeteer.";
    inline static const json parameters_ = json::parse(R"json({
        "type": "object",
        "properties": {
            "command": {
                "type": "string",
                "description": "### Commands\n\n- **navigate**\n  - Navigate to any URL in the browser\n  - Input: `url` (string)\n\n- **screenshot**\n  - Capture screenshots of the entire page or specific elements\n  - Inputs:\n    - `name` (string, required): Name for the screenshot\n    - `selector` (string, optional): CSS selector for element to screenshot\n    - `width` (number, optional, default: 800): Screenshot width\n    - `height` (number, optional, default: 600): Screenshot height\n\n- **click**\n  - Click elements on the page\n  - Input: `selector` (string): CSS selector for element to click\n\n- **hover**\n  - Hover elements on the page\n  - Input: `selector` (string): CSS selector for element to hover\n\n- **fill**\n  - Fill out input fields\n  - Inputs:\n    - `selector` (string): CSS selector for input field\n    - `value` (string): Value to fill\n\n- **select**\n  - Select an element with SELECT tag\n  - Inputs:\n    - `selector` (string): CSS selector for element to select\n    - `value` (string): Value to select\n\n- **evaluate**\n  - Execute JavaScript in the browser console\n  - Input: `script` (string): JavaScript code to execute",
                "enum": [
                    "navigate",
                    "screenshot",
                    "click",
                    "hover",
                    "fill",
                    "select",
                    "evaluate"
                ]
            },
            "url": {
                "type": "string",
                "description": "The URL to navigate to. Required by `navigate`."
            },
            "name": {
                "type": "string",
                "description": "The name of the screenshot. Required by `screenshot`."
            },
            "selector": {
                "type": "string",
                "description": "The CSS selector for the element to interact with. Required by `click`, `hover`, `fill`, and `select`."
            },
            "width": {
                "type": "number",
                "description": "The width of the screenshot. Required by `screenshot`. Default: 800",
                "default": 800
            },
            "height": {
                "type": "number",
                "description": "The height of the screenshot. Required by `screenshot`. Default: 600",
                "default": 600
            },
            "value": {
                "type": "string",
                "description": "The value to fill in input fields. Required by `fill`."
            },
            "script": {
                "type": "string",
                "description": "The JavaScript code to execute. Required by `evaluate`."
            }
        },
        "required": ["command"]
    })json");

    inline static std::set<std::string> allowed_commands = {
        "navigate",
        "screenshot",
        "click",
        "hover",
        "fill",
        "select",
        "evaluate"
    };

    Puppeteer() : BaseMCPTool(name_, description_, parameters_) {}

    ToolResult execute(const json& args) override {
        try {
            if (!_client) {
                return ToolError("Failed to initialize puppeteer client");
            }

            std::string command;
            if (args.contains("command")) {
                if (args["command"].is_string()) {
                    command = args["command"].get<std::string>();
                } else {
                    return ToolError("Invalid command format");
                }
            } else {
                return ToolError("'command' is required");
            }

            if (allowed_commands.find(command) == allowed_commands.end()) {
                return ToolError("Unknown command '" + command + "'. Please use one of the following commands: " + 
                                 std::accumulate(allowed_commands.begin(), allowed_commands.end(), std::string(),
                                                 [](const std::string& a, const std::string& b) {
                                                     return a + (a.empty() ? "" : ", ") + b;
                                                 }));
            }

            json result = _client->call_tool("puppeteer_" + command, args);

            if (result["content"].is_array()) {
                for (size_t i = 0; i < result["content"].size(); i++) {
                    if (result["content"][i]["type"] == "image") {
                        std::string data = result["content"][i]["data"].get<std::string>();
                        std::string mime_type = result["content"][i].value("mimeType", "image/png");
                        // Convert to OAI-compatible image_url format
                        result["content"][i] = {
                            {"type", "image_url"},
                            {"image_url", {{"url", "data:" + mime_type + ";base64," + data}}}
                        };
                    }
                }
            }
            
            bool is_error = result.value("isError", false);
            
            // Return different ToolResult based on whether there is an error
            if (is_error) {
                return ToolError(result.value("content", json::array()));
            } else {
                return ToolResult(result.value("content", json::array()));
            }
        } catch (const std::exception& e) {
            return ToolError(std::string(e.what()));
        }
    }
};

};

#endif // HUMANUS_TOOL_PUPPETEER_H
