#include "llm.h"

namespace humanus {

std::unordered_map<std::string, std::shared_ptr<LLM>> LLM::instances_;

/**
 * @brief Format the message list to the format that LLM can accept
 * @param messages Message object message list
 * @return The formatted message list
 * @throws std::invalid_argument If the message format is invalid or missing necessary fields
 * @throws std::runtime_error If the message type is not supported
 */
json LLM::format_messages(const std::vector<Message>& messages) {
    json formatted_messages = json::array();

    auto concat_content = [](const json& lhs, const json& rhs) -> json {
        if (lhs.is_string() && rhs.is_string()) {
            return lhs.get<std::string>() + "\n" + rhs.get<std::string>(); // Maybe other delimiter?
        }
        json res = json::array();
        if (lhs.is_string()) {
            res.push_back({
                {"type", "text"},
                {"text", lhs.get<std::string>()}
            });
        } else if (lhs.is_array()) {
            res.insert(res.end(), lhs.begin(), lhs.end());
        }
        if (rhs.is_string()) {
            res.push_back({
                {"type", "text"},
                {"text", rhs.get<std::string>()}
            });
        } else if (rhs.is_array()) {
            res.insert(res.end(), rhs.begin(), rhs.end());
        }
        return res;
    };

    for (const auto& message : messages) {
        if (message.content.empty() && message.tool_calls.empty()) {
            continue;
        }
        formatted_messages.push_back(message.to_json());
        if (!llm_config_->enable_tool) {
            if (formatted_messages.back()["content"].is_null()) {
                formatted_messages.back()["content"] = "";
            }
            if (formatted_messages.back()["role"] == "tool") {
                formatted_messages.back()["role"] = "user";
                formatted_messages.back()["content"] = concat_content("Tool result for `" + message.name + "`:\n\n", formatted_messages.back()["content"]);
            } else if (!formatted_messages.back()["tool_calls"].empty()) {
                std::string tool_calls_str = llm_config_->tool_parser.dump(formatted_messages.back()["tool_calls"]);
                formatted_messages.back().erase("tool_calls");
                formatted_messages.back()["content"] = concat_content(formatted_messages.back()["content"], tool_calls_str);
            }
        }
    }

    for (const auto& message : formatted_messages) {
        if (message["role"] != "user" && message["role"] != "assistant" && message["role"] != "system" && message["role"] != "tool") {
            throw std::invalid_argument("Invalid role: " + message["role"].get<std::string>());
        }
    }
    
    size_t i = 0, j = -1;
    for (; i < formatted_messages.size(); i++) {
        if (i == 0 || formatted_messages[i]["role"] != formatted_messages[j]["role"]) {
            formatted_messages[++j] = formatted_messages[i];
        } else {
            formatted_messages[j]["content"] = concat_content(formatted_messages[j]["content"], formatted_messages[i]["content"]);
            if (!formatted_messages[i]["tool_calls"].empty()) {
                formatted_messages[j]["tool_calls"] = concat_content(formatted_messages[j]["tool_calls"], formatted_messages[i]["tool_calls"]);
            }
        }
    }

    formatted_messages.erase(formatted_messages.begin() + j + 1, formatted_messages.end());

    if (!llm_config_->enable_vision) {
        for (auto& message : formatted_messages) {
            message["content"] = parse_json_content(message["content"]); // Images will be replaced by [image1], [image2], ...
        }
    }

    return formatted_messages;
}

std::string LLM::ask(
    const std::vector<Message>& messages,
    const std::string& system_prompt,
    const std::string& next_step_prompt,
    int max_retries
) {
    json formatted_messages = json::array();

    if (!system_prompt.empty()) {
        formatted_messages.push_back({
            {"role", "system"},
            {"content", system_prompt}
        });
    }
    
    json _formatted_messages = format_messages(messages);
    formatted_messages.insert(formatted_messages.end(), _formatted_messages.begin(), _formatted_messages.end());

    if (!next_step_prompt.empty()) {
        if (formatted_messages.empty() || formatted_messages.back()["role"] != "user") {
            formatted_messages.push_back({
                {"role", "user"},
                {"content", next_step_prompt}
            });
        } else {
            if (formatted_messages.back()["content"].is_string()) {
                formatted_messages.back()["content"] = formatted_messages.back()["content"].get<std::string>() + "\n\n" + next_step_prompt;
            } else if (formatted_messages.back()["content"].is_array()) {
                formatted_messages.back()["content"].push_back({
                    {"type", "text"},
                    {"text", next_step_prompt}
                });
            }
        }
    }

    json body = {
        {"model", llm_config_->model},
        {"messages", formatted_messages}
    };

    if (llm_config_->temperature > 0) {
        body["temperature"] = llm_config_->temperature;
    }

    if (llm_config_->max_tokens > 0) {
        body["max_tokens"] = llm_config_->max_tokens;
    }
    
    std::string body_str = body.dump();

    int retry = 0;

    while (retry <= max_retries) {
        // send request
        auto res = client_->Post(llm_config_->endpoint, body_str, "application/json");

        if (!res) {
            logger->error(std::string(__func__) + ": Failed to send request: " + httplib::to_string(res.error()));
        } else if (res->status == 200) {
            try {
                json json_data = json::parse(res->body);
                total_prompt_tokens_ += json_data["usage"]["prompt_tokens"].get<size_t>();
                total_completion_tokens_ += json_data["usage"]["completion_tokens"].get<size_t>();
                return json_data["choices"][0]["message"]["content"].get<std::string>();
            } catch (const std::exception& e) {
                logger->error(std::string(__func__) + ": Failed to parse response: error=" + std::string(e.what()) + ", body=" + res->body);
            }
        } else {
            logger->error(std::string(__func__) + ": Failed to send request: status=" + std::to_string(res->status) + ", body=" + res->body);
        }

        retry++;

        if (retry > max_retries) {
            break;
        }

        // wait for a while before retrying
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        logger->info("Retrying " + std::to_string(retry) + "/" + std::to_string(max_retries));
    }

    // If the logger has a file sink, log the request body
    if (logger->sinks().size() > 1) {
        auto file_sink = std::dynamic_pointer_cast<spdlog::sinks::basic_file_sink_mt>(logger->sinks()[1]);
        if (file_sink) {
            file_sink->log(spdlog::details::log_msg(
                spdlog::source_loc{},
                logger->name(),
                spdlog::level::debug,
                "Failed to get response from LLM. Full request body: " + body_str
            ));
        }
    }

    throw std::runtime_error("Failed to get response from LLM");
}

json LLM::ask_tool(
    const std::vector<Message>& messages,
    const std::string& system_prompt,
    const std::string& next_step_prompt,
    const json& tools,
    const std::string& tool_choice,
    int max_retries
) {
    if (tool_choice != "none" && tool_choice != "auto" && tool_choice != "required") {
        throw std::invalid_argument("Invalid tool_choice: " + tool_choice);
    }

    json formatted_messages = json::array();

    if (!system_prompt.empty()) {
        formatted_messages.push_back({
            {"role", "system"},
            {"content", system_prompt}
        });
    }
    
    json _formatted_messages = format_messages(messages);
    formatted_messages.insert(formatted_messages.end(), _formatted_messages.begin(), _formatted_messages.end());

    if (!next_step_prompt.empty()) {
        if (formatted_messages.empty() || formatted_messages.back()["role"] != "user") {
            formatted_messages.push_back({
                {"role", "user"},
                {"content", next_step_prompt}
            });
        } else {
            if (formatted_messages.back()["content"].is_string()) {
                formatted_messages.back()["content"] = formatted_messages.back()["content"].get<std::string>() + "\n\n" + next_step_prompt;
            } else if (formatted_messages.back()["content"].is_array()) {
                formatted_messages.back()["content"].push_back({
                    {"type", "text"},
                    {"text", next_step_prompt}
                });
            }
        }
    }

    if (!tools.empty()) {
        for (const json& tool : tools) {
            if (!tool.contains("type")) {
                throw std::invalid_argument("Tool must contain 'type' field but got: " + tool.dump(2));
            }
        }
        if (tool_choice == "required" && tools.empty()) {
            throw std::invalid_argument("No tool available for required tool choice");
        }
        if (!tools.is_array()) {
            throw std::invalid_argument("Tools must be an array");
        }
    }
    
    json body = {
        {"model", llm_config_->model},
        {"messages", formatted_messages}
    };

    if (llm_config_->temperature > 0) {
        body["temperature"] = llm_config_->temperature;
    }

    if (llm_config_->max_tokens > 0) {
        body["max_tokens"] = llm_config_->max_tokens;
    }

    if (llm_config_->enable_tool) {
        body["tools"] = tools;
        body["tool_choice"] = tool_choice;
    } else {
        if (body["messages"].empty() || body["messages"].back()["role"] != "user") {
            body["messages"].push_back({
                {"role", "user"},
                {"content", llm_config_->tool_parser.hint(tools.dump(2))}
            });
        } else if (body["messages"].back()["content"].is_string()) {
            body["messages"].back()["content"] = body["messages"].back()["content"].get<std::string>() + "\n\n" + llm_config_->tool_parser.hint(tools.dump(2));
        } else if (body["messages"].back()["content"].is_array()) {
            body["messages"].back()["content"].push_back({
                {"type", "text"},
                {"text", llm_config_->tool_parser.hint(tools.dump(2))}
            });
        }
    }
    
    std::string body_str = body.dump();

    int retry = 0;

    while (retry <= max_retries) {
        // send request
        auto res = client_->Post(llm_config_->endpoint, body_str, "application/json");

        if (!res) {
            logger->error(std::string(__func__) + ": Failed to send request: " + httplib::to_string(res.error()));
        } else if (res->status == 200) {
            try {
                json json_data = json::parse(res->body);
                json message = json_data["choices"][0]["message"];
                if (!llm_config_->enable_tool && message["content"].is_string()) {
                    message = llm_config_->tool_parser.parse(message["content"].get<std::string>());
                }
                total_prompt_tokens_ += json_data["usage"]["prompt_tokens"].get<size_t>();
                total_completion_tokens_ += json_data["usage"]["completion_tokens"].get<size_t>();
                return message;
            } catch (const std::exception& e) {
                logger->error(std::string(__func__) + ": Failed to parse response: error=" + std::string(e.what()) + ", body=" + res->body);
            }
        } else {
            logger->error(std::string(__func__) + ": Failed to send request: status=" + std::to_string(res->status) + ", body=" + res->body);
        }

        retry++;

        if (retry > max_retries) {
            break;
        }

        // wait for a while before retrying
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        logger->info("Retrying " + std::to_string(retry) + "/" + std::to_string(max_retries));
    }

    // If the logger has a file sink, log the request body
    for (const auto& sink : logger->sinks()) {
        auto file_sink = std::dynamic_pointer_cast<spdlog::sinks::basic_file_sink_mt>(sink);
        if (file_sink) {
            file_sink->log(spdlog::details::log_msg(
                spdlog::source_loc{},
                logger->name(),
                spdlog::level::debug,
                "Failed to get response from LLM. Full request body: " + body_str
            ));
        }
        auto stderr_sink = std::dynamic_pointer_cast<spdlog::sinks::stderr_color_sink_mt>(sink);
        if (stderr_sink) {
            stderr_sink->log(spdlog::details::log_msg(
                spdlog::source_loc{},
                logger->name(),
                spdlog::level::debug,
                "Failed to get response from LLM. See log file for full request body."
            ));
        }
        auto session_sink = std::dynamic_pointer_cast<SessionSink>(sink);
        if (session_sink) {
            session_sink->log(spdlog::details::log_msg(
                spdlog::source_loc{},
                logger->name(),
                spdlog::level::debug,
                "Failed to get response from LLM. See log file for full request body."
            ));
        }
    }

    throw std::runtime_error("Failed to get response from LLM");
}

} // namespace humanus