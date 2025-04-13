#ifndef HUMANUS_MEMORY_UTILS_H
#define HUMANUS_MEMORY_UTILS_H

#include "../include/utils.h"
#include "schema.h"
#include "llm.h"

namespace humanus {

// Removes enclosing code block markers ```[language] and ``` from a given string.
// 
// Remarks:
// - The function uses a regex pattern to match code blocks that may start with ``` followed by an optional language tag (letters or numbers) and end with ```.
// - If a code block is detected, it returns only the inner content, stripping out the markers.
// - If no code block markers are found, the original content is returned as-is.
inline std::string remove_code_blocks(const std::string& text) {
    static const std::regex pattern(R"(^```[a-zA-Z0-9]*\n([\s\S]*?)\n```$)");
    std::smatch match;
    if (std::regex_search(text, match, pattern)) {
        return match[1].str();
    }
    return text;
}

inline size_t get_uuid_64() {
    const std::string chars = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    
    unsigned long long int uuid_int = 0;
    for (int i = 0; i < 16; ++i) {
        uuid_int = (uuid_int << 4) | dis(gen);
    }
    
    // RFC 4122 variant
    uuid_int &= ~(0xc000ULL << 48);
    uuid_int |= 0x8000ULL << 48;
    
    // version 4, random UUID
    int version = 4;
    uuid_int &= ~(0xfULL << 12);
    uuid_int |= static_cast<unsigned long long>(version) << 12;
    
    return uuid_int;
}

std::string get_update_memory_messages(const json& old_memories, const json& new_facts, const std::string& update_memory_prompt);

// Get the description of the image
// image_url should be like: data:{mime_type};base64,{base64_data}
inline std::string get_image_description(const std::string& image_url, const std::shared_ptr<LLM>& llm, const std::string& vision_details) {
    if (!llm) {
        return "Here is an image failed to get description due to missing LLM instance.";
    }

    json content = json::array({
        {
            {"type", "text"},
            {"text", "A user is providing an image. Provide a high level description of the image and do not include any additional text."}
        },
        {
            {"type", "image_url"},
            {"image_url", {
                {"url", image_url},
                {"detail", vision_details}
            }}
        }
    });
    return llm->ask(
        {Message::user_message(content)}
    );
}

// Parse the vision messages from the messages
inline Message parse_vision_message(const Message& message, const std::shared_ptr<LLM>& llm = nullptr, const std::string& vision_details = "auto") {
    Message returned_message = message;

    if (returned_message.content.is_array()) {
        // Multiple image URLs in content
        for (auto& content_item : returned_message.content) {
            if (content_item["type"] == "image_url") {
                auto description = get_image_description(content_item["image_url"]["url"], llm, vision_details);
                content_item = description;
            }
        }
    } else if (returned_message.content.is_object() && returned_message.content["type"] == "image_url") {
        auto image_url = returned_message.content["image_url"]["url"];
        returned_message.content = get_image_description(image_url, llm, vision_details);
    }

    return returned_message;
}

}

#endif // HUMANUS_MEMORY_UTILS_H