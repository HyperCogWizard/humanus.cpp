#ifndef HUMANUS_TOOL_BASE_H
#define HUMANUS_TOOL_BASE_H

#include "schema.h"
#include "config.h"
#include "utils.h"
#include "mcp_stdio_client.h"
#include "mcp_sse_client.h"
#include <string>

namespace humanus {

// Represents the result of a tool execution.
struct ToolResult {
    json output;
    json error;
    json system;

    ToolResult(const json& output, const json& error = {}, const json& system = {}) 
        : output(output), error(error), system(system) {}

    bool empty() const {
        return output.empty() && error.empty() && system.empty();
    }

    ToolResult operator+(const ToolResult& other) const {
        auto combined_field = [](const json& field, const json& other_field) {
            if (field.empty()) {
                return other_field;
            }
            if (other_field.empty()) {
                return field;
            }
            json result = json::array();
            if (field.is_array()) {
                result.insert(result.end(), field.begin(), field.end());
            } else  {
                result.push_back(field);
            }
            if (other_field.is_array()) {
                result.insert(result.end(), other_field.begin(), other_field.end());
            } else  {
                result.push_back(other_field);
            }
            return result;
        };

        return {
            combined_field(output, other.output),
            combined_field(error, other.error),
            combined_field(system, other.system)
        };
    }

    std::string to_string(int max_length = -1) const {
        std::string result;
        if (!error.empty()) {
            result = "Error: " + parse_json_content(error);
        } else {
            result = parse_json_content(output);
        }
        if (max_length > 0 && result.length() > max_length) {
            result = result.substr(0, max_length) + "...";
        }
        return result;
    }
};

// A ToolResult that represents a failure.
struct ToolError : ToolResult {
    ToolError(const json& error) : ToolResult({}, error) {}
};

struct BaseTool {
    std::string name;
    std::string description;
    json parameters;

    BaseTool(const std::string& name, const std::string& description, const json& parameters) :
        name(name), description(description), parameters(parameters) {}

    // Execute the tool with given parameters.
    ToolResult operator()(const json& arguments) {
        return execute(arguments);
    }

    // Execute the tool with given parameters.
    virtual ToolResult execute(const json& arguments) = 0;

    json to_param() const {
        return {
            {"type", "function"},
            {"function", {
                {"name", name},
                {"description", description},
                {"parameters", parameters}
            }}
        };
    }
};

// Execute the tool with given parameters.
struct BaseMCPTool : BaseTool {
    std::shared_ptr<mcp::client> _client;

    BaseMCPTool(const std::string& name, const std::string& description, const json& parameters, const std::shared_ptr<mcp::client>& client)
    : BaseTool(name, description, parameters), _client(client) {}

    BaseMCPTool(const std::string& name, const std::string& description, const json& parameters)
    : BaseTool(name, description, parameters), _client(create_client(name)) {}

    static std::shared_ptr<mcp::client> create_client(const std::string& server_name) {
        std::shared_ptr<mcp::client> _client;
        try {
            // Load tool configuration from config file
            auto _config = Config::get_mcp_server_config(server_name);

            if (_config.type == "stdio") {
                std::string command = _config.command;
                if (!_config.args.empty()) {
                    for (const auto& arg : _config.args) {
                        command += " " + arg;
                    }
                }
                _client = std::make_shared<mcp::stdio_client>(command, _config.env_vars);   
            } else if (_config.type == "sse") {
                if (!_config.host.empty() && _config.port > 0) {
                    _client = std::make_shared<mcp::sse_client>(_config.host, _config.port);
                } else if (!_config.url.empty()) {
                    _client = std::make_shared<mcp::sse_client>(_config.url, "/sse");
                } else {
                    throw std::runtime_error("MCP SSE configuration missing host or port or url");
                }
            }

            _client->initialize(server_name + "_client", "0.1.0");
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to initialize MCP tool client for `" + server_name + "`: " + std::string(e.what()));
        }
        return _client;
    }
    
    ToolResult execute(const json& arguments) override {
        try {
            if (!_client) {
                throw std::runtime_error("MCP client not initialized");
            }
            json result = _client->call_tool(name, arguments);
            bool is_error = result.value("isError", false);
            // Return different ToolResult based on whether there is an error
            if (is_error) {
                return ToolError(result.value("content", json::array()));
            } else {
                return ToolResult(result.value("content", json::array()));
            }
        } catch (const std::exception& e) {
            return ToolError(e.what());
        }
    }
};

}

#endif // HUMANUS_TOOL_BASE_H
