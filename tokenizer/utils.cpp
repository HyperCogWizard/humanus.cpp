#include "utils.h"

namespace humanus {

int num_tokens_from_messages(const BaseTokenizer& tokenizer, const json& messages)  {
    // TODO: configure the magic number
    static const int tokens_per_message = 3;
    static const int tokens_per_name = 1;
    static const int tokens_per_image = 1024;

    int num_tokens = 0;

    json messages_;

    if (messages.is_object()) {
        messages_ = json::array({messages});
    } else {
        messages_ = messages;
    }

    for (const auto& message : messages_) {
        num_tokens += tokens_per_message;
        for (const auto& [key, value] : message.items()) {
            if (value.is_string()) {
                num_tokens += tokenizer.encode(value.get<std::string>()).size();
            } else if (value.is_array()) {
                for (const auto& item : value) {
                    if (item.contains("text")) {
                        num_tokens += tokenizer.encode(item.at("text").get<std::string>()).size();
                    } else if (item.contains("image_url")) {
                        num_tokens += tokens_per_image;
                    }
                }
            }
            if (key == "name") {
                num_tokens += tokens_per_name;
            }
        }
    }
    num_tokens += 3;  // every reply is primed with <|start|>assistant<|message|>
    return num_tokens;
}

int num_tokens_for_tools(const BaseTokenizer& tokenizer, const json& tools, const json& messages) {
    // TODO: configure the magic number
    static const int tool_init = 10;
    static const int prop_init = 3;
    static const int prop_key = 3;
    static const int enum_init = 3;
    static const int enum_item = 3;
    static const int tool_end = 12;

    int tool_token_count = 0;
    if (!tools.empty()) {
        for (const auto& tool : tools) {
            tool_token_count += tool_init; // Add tokens for start of each tool
            auto function = tool["function"];
            auto f_name = function["name"].get<std::string>();
            auto f_desc = function["description"].get<std::string>();
            if (f_desc.back() == '.') {
                f_desc.pop_back();
            }
            auto line = f_name + ":" + f_desc;
            tool_token_count += tokenizer.encode(line).size();

            if (function["parameters"].contains("properties")) {
                tool_token_count += prop_init; // Add tokens for start of each property
                for (const auto& [key, value] : function["parameters"]["properties"].items()) {
                    tool_token_count += prop_key; // Add tokens for each set property
                    auto p_name = key;
                    auto p_type = value["type"].get<std::string>();
                    auto p_desc = value["description"].get<std::string>();

                    if (value.contains("enum")) {
                        tool_token_count += enum_init; // Add tokens if property has enum list
                        for (const auto& item : value["enum"]) {
                            tool_token_count += enum_item;
                            tool_token_count += tokenizer.encode(item.get<std::string>()).size();
                        }
                    }

                    if (p_desc.back() == '.') {
                        p_desc.pop_back();
                    }
                    auto line = p_name + ":" + p_type + ":" + p_desc;
                    tool_token_count += tokenizer.encode(line).size();
                }
            }
        }
        tool_token_count += tool_end;
    }

    auto messages_token_count = num_tokens_from_messages(tokenizer, messages);
    auto total_token_count = tool_token_count + messages_token_count;

    return total_token_count;
}

}