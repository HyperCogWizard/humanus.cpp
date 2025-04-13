#ifndef HUMANUS_TOOL_PLAYWRIGHT_H
#define HUMANUS_TOOL_PLAYWRIGHT_H

#include "base.h"

namespace humanus {

struct Playwright : BaseMCPTool {
    inline static const std::string name_ = "playwright";
    inline static const std::string description_ = "Interact with web pages, take screenshots, generate test code, web scraps the page and execute JavaScript in a real browser environment. Note: Most of the time you need to observer the page before executing other actions.";
    inline static const json parameters_ = json::parse(R"json({
        "type": "object",
        "properties": {
            "command": {
                "type": "string",
                "enum": [
                    "navigate",
                    "screenshot",
                    "click",
                    "iframe_click",
                    "fill",
                    "select",
                    "hover",
                    "evaluate",
                    "console_logs",
                    "close",
                    "get",
                    "post",
                    "put",
                    "patch",
                    "delete",
                    "expect_response",
                    "assert_response",
                    "custom_user_agent",
                    "get_visible_text",
                    "get_visible_html",
                    "go_back",
                    "go_forward",
                    "drag",
                    "press_key",
                    "save_as_pdf"
                ],
                "description": "Specify the command to perform on the web page using Playwright."
            },
            "url": {
                "type": "string",
                "description": "URL to navigate to, or to perform HTTP operations on. **Required by**: `navigate`, `get`, `post`, `put`, `patch`, `delete`, `expect_response`."
            },
            "selector": {
                "type": "string",
                "description": "CSS selector for the element to interact with. Note: Use JS to determine available selectors first. **Required by**: `click`, `iframe_click`, `fill`, `select`, `hover`, `drag`, `press_key`."
            },
            "name": {
                "type": "string",
                "description": "Name for the screenshot or file operations. **Required by**: `screenshot`."
            },
            "browserType": {
                "type": "string",
                "enum": [
                    "chromium",
                    "firefox",
                    "webkit"
                ],
                "description": "Browser type to use. Defaults to chromium. **Used by**: `navigate`."
            },
            "width": {
                "type": "number",
                "description": "Viewport width in pixels. Defaults to 1280. **Used by**: `navigate`, `screenshot`."
            },
            "height": {
                "type": "number",
                "description": "Viewport height in pixels. Defaults to 720. **Used by**: `navigate`, `screenshot`."
            },
            "timeout": {
                "type": "number",
                "description": "Navigation or operation timeout in milliseconds. **Used by**: `navigate`."
            },
            "waitUntil": {
                "type": "string",
                "enum": [
                    "load",
                    "domcontentloaded",
                    "networkidle",
                    "commit"
                ],
                "description": "Navigation wait condition. **Used by**: `navigate`."
            },
            "headless": {
                "type": "boolean",
                "description": "Run browser in headless mode. Defaults to false. **Used by**: `navigate`."
            },
            "fullPage": {
                "type": "boolean",
                "description": "Capture the entire page. Defaults to false. **Used by**: `screenshot`."
            },
            "savePng": {
                "type": "boolean",
                "description": "Save the screenshot as a PNG file. Defaults to false. **Used by**: `screenshot`."
            },
            "storeBase64": {
                "type": "boolean",
                "description": "Store screenshot in base64 format. Defaults to true. **Used by**: `screenshot`."
            },
            "downloadsDir": {
                "type": "string",
                "description": "Path to save the file. Defaults to user's Downloads folder. **Used by**: `screenshot`."
            },
            "iframeSelector": {
                "type": "string",
                "description": "CSS selector for the iframe containing the element to click. **Required by**: `iframe_click`."
            },
            "value": {
                "type": "string",
                "description": "Value to fill in an input or select in a dropdown. **Required by**: `fill`, `select`."
            },
            "sourceSelector": {
                "type": "string",
                "description": "CSS selector for the source element to drag. **Required by**: `drag`."
            },
            "targetSelector": {
                "type": "string",
                "description": "CSS selector for the target location to drag to. **Required by**: `drag`."
            },
            "key": {
                "type": "string",
                "description": "Key to press on the keyboard. **Required by**: `press_key`."
            },
            "outputPath": {
                "type": "string",
                "description": "Directory path where the PDF will be saved. **Required by**: `save_as_pdf`."
            },
            "filename": {
                "type": "string",
                "description": "Name of the PDF file. Defaults to `page.pdf`. **Used by**: `save_as_pdf`."
            },
            "format": {
                "type": "string",
                "description": "Page format, e.g., 'A4', 'Letter'. **Used by**: `save_as_pdf`."
            },
            "printBackground": {
                "type": "boolean",
                "description": "Whether to print background graphics. **Used by**: `save_as_pdf`."
            },
            "margin": {
                "type": "object",
                "properties": {
                    "top": {
                        "type": "string"
                    },
                    "right": {
                        "type": "string"
                    },
                    "bottom": {
                        "type": "string"
                    },
                    "left": {
                        "type": "string"
                    }
                },
                "description": "Margins of the page. **Used by**: `save_as_pdf`."
            }
        },
        "required": ["command"]
    })json");

    inline static std::set<std::string> allowed_commands = {
        "navigate",
        "screenshot",
        "click",
        "iframe_click",
        "fill",
        "select",
        "hover",
        "evaluate",
        "console_logs",
        "close",
        "get",
        "post",
        "put",
        "patch",
        "delete",
        "expect_response",
        "assert_response",
        "custom_user_agent",
        "get_visible_text",
        "get_visible_html",
        "go_back",
        "go_forward",
        "drag",
        "press_key",
        "save_as_pdf"
    };

    Playwright() : BaseMCPTool(name_, description_, parameters_) {}

    ToolResult execute(const json& args) override {
        try {
            if (!_client) {
                return ToolError("Failed to initialize playwright client");
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

            json result = _client->call_tool("playwright_" + command, args);

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

} // namespace humanus

#endif // HUMANUS_TOOL_PLAYWRIGHT_H