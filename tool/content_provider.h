#ifndef HUMANUS_TOOL_CONTENT_PROVIDER_H
#define HUMANUS_TOOL_CONTENT_PROVIDER_H

#include "base.h"
#include "utils.h"
#include <vector>
#include <map>

namespace humanus {

struct ContentProvider : BaseTool {
    inline static const std::string name_ = "content_provider";
    inline static const std::string description_ = "Use this tool to save temporary content for later use. For example, you can save a large code file (like HTML) and read it by chunks later.";
    inline static const json parameters_ = json::parse(R"json(
        {
            "type": "object",
            "properties": {
                "operation": {
                    "type": "string",
                    "description": "The operation to perform: `write` to save content, `read` to retrieve content",
                    "enum": ["write", "read"]
                },
                "content": {
                    "type": "array",
                    "description": "The content to store. Required when operation is `write` (the `read` operation will return the same format). Format: [{'type': 'text', 'text': <content>}, {'type': 'image_url', 'image_url': {'url': <image_url>}}]",
                    "items": {
                        "type": "object",
                        "properties": {
                            "type": {
                                "type": "string",
                                "enum": ["text", "image_url"]
                            },
                            "text": {
                                "type": "string",
                                "description": "Text content. Required when type is `text`."
                            },
                            "image_url": {
                                "type": "object",
                                "description": "Image URL information. Required when type is `image_url`.",
                                "properties": {
                                    "url": {
                                        "type": "string",
                                        "description": "URL of the image"
                                    }
                                }
                            }
                        }
                    }
                },
                "cursor": {
                    "type": "string",
                    "description": "The cursor position for reading content. Required when operation is `read`. Use `start` for the beginning or the cursor returned from a previous read."
                },
                "max_chunk_size": {
                    "type": "integer",
                    "description": "Maximum size in characters for each text chunk. Default is 4000. Used by `write` operation.",
                    "default": 4000
                }
            },
            "required": ["operation"]
        }
    )json");

    inline static std::map<std::string, std::vector<json>> content_store_;
    inline static size_t MAX_STORE_ID = 100;
    inline static size_t current_id_ = 0;

    ContentProvider() : BaseTool(name_, description_, parameters_) {}

    std::vector<json> split_text_into_chunks(const std::string& text, int max_chunk_size) {
        std::vector<json> chunks;
        
        if (text.empty()) {
            return chunks;
        }
        
        size_t text_length = text.length();
        size_t offset = 0;
        
        while (offset < text_length) {
            size_t raw_chunk_size = std::min(static_cast<size_t>(max_chunk_size), text_length - offset);
            
            // Make sure the chunk is valid UTF-8
            std::string potential_chunk = text.substr(offset, raw_chunk_size);
            size_t valid_utf8_length = validate_utf8(potential_chunk);
            
            // Adjust the chunk size to a valid UTF-8 character boundary
            size_t chunk_size = valid_utf8_length;
            
            // If not at the end of the text and we didn't reduce the chunk size due to UTF-8 truncation,
            // try to split at a natural break point (space, newline, punctuation)
            if (offset + chunk_size < text_length && chunk_size == raw_chunk_size) {
                size_t break_pos = offset + chunk_size;
                
                // Search backward for a natural break point (space, newline, punctuation)
                size_t min_pos = offset + valid_utf8_length / 2; // Don't search too far, at least keep half of the valid content
                while (break_pos > min_pos && 
                       text[break_pos] != ' ' && 
                       text[break_pos] != '\n' && 
                       text[break_pos] != '.' && 
                       text[break_pos] != ',' && 
                       text[break_pos] != ';' && 
                       text[break_pos] != ':' && 
                       text[break_pos] != '!' && 
                       text[break_pos] != '?') {
                    break_pos--;
                }
                
                // If a suitable break point is found and it's not the original position
                if (break_pos > min_pos) {
                    break_pos++; // Include the last character
                    std::string new_chunk = text.substr(offset, break_pos - offset);
                    size_t new_valid_length = validate_utf8(new_chunk); // Validate the new chunk
                    chunk_size = break_pos - offset;
                }
            }
            
            // Create a text chunk
            json chunk;
            chunk["type"] = "text";
            chunk["text"] = text.substr(offset, chunk_size);
            chunks.push_back(chunk);
            
            offset += chunk_size;
        }
        
        return chunks;
    }

    // Handle write operation
    ToolResult handle_write(const json& args) {
        int max_chunk_size = args.value("max_chunk_size", 4000);
        
        if (!args.contains("content") || !args["content"].is_array()) {
            return ToolError("`content` is required and must be an array");
        }
        
        std::vector<json> processed_content;

        std::string text_content;
        
        // Process content, split large text or mixture of text and image
        for (const auto& item : args["content"]) {
            if (!item.contains("type")) {
                return ToolError("Each content item must have a `type` field");
            }
            
            std::string type = item["type"].get<std::string>();
            
            if (type == "text") {
                if (!item.contains("text") || !item["text"].is_string()) {
                    return ToolError("Text items must have a `text` field with string value");
                }
                
                text_content += item["text"].get<std::string>() + "\n\n"; // Handle them together
            } else if (type == "image_url") {
                if (!text_content.empty()) {
                    auto chunks = split_text_into_chunks(text_content, max_chunk_size);
                    processed_content.insert(processed_content.end(), chunks.begin(), chunks.end());
                    text_content.clear();
                }

                if (!item.contains("image_url") || !item["image_url"].is_object() || 
                    !item["image_url"].contains("url") || !item["image_url"]["url"].is_string()) {
                    return ToolError("Image items must have an `image_url` field with a `url` property");
                }
                
                // Image remains as a whole
                processed_content.push_back(item);
            } else {
                return ToolError("Unsupported content type: " + type);
            }
        }

        if (!text_content.empty()) {
            auto chunks = split_text_into_chunks(text_content, max_chunk_size);
            processed_content.insert(processed_content.end(), chunks.begin(), chunks.end());
            text_content.clear();
        }
        
        // Generate a unique store ID
        std::string store_id = "content_" + std::to_string(current_id_);

        if (content_store_.find(store_id) != content_store_.end()) {
            logger->warn("Store ID `" + store_id + "` already exists, it will be overwritten");
        }

        current_id_ = (current_id_ + 1) % MAX_STORE_ID;
        
        // Store processed content
        content_store_[store_id] = processed_content;
        
        // Return store ID and number of content items
        json result;
        result["store_id"] = store_id;
        result["total_items"] = processed_content.size();
        
        return ToolResult(result.dump(2));;
    }

    // Handle read operation
    ToolResult handle_read(const json& args) {
        if (!args.contains("cursor") || !args["cursor"].is_string()) {
            return ToolError("`cursor` is required for read operations");
        }
        
        std::string cursor = args["cursor"];
        
        if (cursor == "start") {
            // List all available store IDs
            json available_stores = json::array();
            for (const auto& [id, content] : content_store_) {
                json store_info;
                store_info["store_id"] = id;
                store_info["total_items"] = content.size();
                available_stores.push_back(store_info);
            }
            
            if (available_stores.empty()) {
                return ToolResult("No content available. Use `write` operation to store content first.");
            }
            
            json result;
            result["available_stores"] = available_stores;
            result["next_cursor"] = "select_store";
            
            return ToolResult(result.dump(2));
        } else if (cursor == "select_store") {
            // User needs to select a store ID
            return ToolError("Please provide a store_id as cursor in format `content_X:Y`");
        } else if (cursor.find(":") != std::string::npos) { // content_X:Y
            // User is browsing specific content in a store
            size_t delimiter_pos = cursor.find(":");
            std::string store_id = cursor.substr(0, delimiter_pos);
            size_t index = std::stoul(cursor.substr(delimiter_pos + 1));
            
            if (content_store_.find(store_id) == content_store_.end()) {
                return ToolError("Store ID `" + store_id + "` not found");
            }
            
            if (index >= content_store_[store_id].size()) {
                return ToolError("Index out of range");
            }
            
            // Return the requested content item
            json result = content_store_[store_id][index];
            
            // Add navigation information
            if (index + 1 < content_store_[store_id].size()) {
                result["next_cursor"] = store_id + ":" + std::to_string(index + 1);
                result["remaining_items"] = content_store_[store_id].size() - index - 1;
            } else {
                result["next_cursor"] = "end";
                result["remaining_items"] = 0;
            }
            
            return ToolResult(result.dump(2));
        } else if (cursor == "end") {
            return ToolResult("You have reached the end of the content.");
        } else {
            return ToolError("Invalid cursor format");
        }
    }

    ToolResult execute(const json& args) override {
        try {
            if (!args.contains("operation")) {
                return ToolError("`operation` is required");
            }
            
            std::string operation = args["operation"];
            
            if (operation == "write") {
                return handle_write(args);
            } else if (operation == "read") {
                return handle_read(args);
            } else {
                return ToolError("Unknown operation `" + operation + "`. Please use `write` or `read`");
            }
        } catch (const std::exception& e) {
            return ToolError(std::string(e.what()));
        }
    }
};

} // namespace humanus

#endif // HUMANUS_TOOL_CONTENT_PROVIDER_H