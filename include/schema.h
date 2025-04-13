#ifndef HUMANUS_SCHEMA_H
#define HUMANUS_SCHEMA_H

#include "mcp_message.h"
#include "utils.h"
#include "httplib.h"
#include "tokenizer/utils.h"
#include "tokenizer/bpe.h"
#include <string>
#include <vector>
#include <map>
#include <algorithm>

namespace humanus {

using json = mcp::json;

// Agent execution states
enum class AgentState {
    IDLE = 0,
    RUNNING = 1,
    FINISHED = 2,
    ERR = 3 // Don't use ERROR
};

extern std::map<AgentState, std::string> agent_state_map;

struct Function {
    std::string name;
    json arguments;

    json to_json() const {
        json function;
        function["name"] = name;
        function["arguments"] = arguments;
        return function;
    }
    
    bool empty() const {
        return name.empty() && arguments.empty();
    }
};

// Represents a tool/function call in a message
struct ToolCall {
    std::string id;
    std::string type;
    Function function;

    bool empty() const {
        return id.empty() && type.empty() && function.empty();
    }

    json to_json() const {
        json tool_call;
        tool_call["id"] = id;
        tool_call["type"] = type;
        tool_call["function"] = function.to_json();
        return tool_call;
    }

    static ToolCall from_json(const json& tool_call_json) {
        ToolCall tool_call;
        tool_call.id = tool_call_json["id"];
        tool_call.type = tool_call_json["type"];
        tool_call.function.name = tool_call_json["function"]["name"];
        tool_call.function.arguments = tool_call_json["function"]["arguments"];
        return tool_call;
    }

    static std::vector<ToolCall> from_json_list(const json& tool_calls_json) {
        std::vector<ToolCall> tool_calls;
        for (const auto& tool_call_json : tool_calls_json) {
            tool_calls.push_back(from_json(tool_call_json));
        }
        return tool_calls;
    }
};

// Represents a chat message in the conversation
struct Message {
    std::string role;
    json content;
    std::string name;
    std::string tool_call_id;
    std::vector<ToolCall> tool_calls;
    int num_tokens;

    // TODO: configure the tokenizer
    inline static const std::shared_ptr<BaseTokenizer> tokenizer = std::make_shared<BPETokenizer>((PROJECT_ROOT / "tokenizer" / "cl100k_base.tiktoken").string()); // use cl100k_base to roughly count tokens

    Message(const std::string& role, const json& content, const std::string& name = "", const std::string& tool_call_id = "", const std::vector<ToolCall> tool_calls = {})
    : role(role), content(content), name(name), tool_call_id(tool_call_id), tool_calls(tool_calls) {
        num_tokens = num_tokens_from_messages(*tokenizer, to_json());
    }

    std::vector<Message> operator+(const Message& other) const {
        return {*this, other};
    }

    std::vector<Message> operator+(const std::vector<Message>& other) const {
        std::vector<Message> result;
        result.reserve(1 + other.size());
        result.push_back(*this);
        result.insert(result.end(), other.begin(), other.end());
        return result;
    }

    friend std::vector<Message> operator+(const std::vector<Message>& lhs, const Message& rhs) {
        std::vector<Message> result;
        result.reserve(lhs.size() + 1);
        result.insert(result.end(), lhs.begin(), lhs.end());
        result.push_back(rhs);
        return result;
    }

    friend std::vector<Message> operator+(const std::vector<Message>& lhs, const std::vector<Message>& rhs) {
        std::vector<Message> result;
        result.reserve(lhs.size() + rhs.size());
        result.insert(result.end(), lhs.begin(), lhs.end());
        result.insert(result.end(), rhs.begin(), rhs.end());
        return result;
    }

    // Convert message to dictionary format
    json to_json() const {
        json message;
        message["role"] = role;
        if (!content.empty()) {
            message["content"] = content;
        }
        if (!tool_calls.empty()) {
            message["tool_calls"] = json::array();
            for (const auto& call : tool_calls) {
                message["tool_calls"].push_back(call.to_json());
            }
        }
        if (!name.empty()) {
            message["name"] = name;
        }
        if (!tool_call_id.empty()) {
            message["tool_call_id"] = tool_call_id;
        }
        return message;
    }

    // Convert message to dictionary format
    json to_dict() const {
        return to_json();
    }

    static Message user_message(const json& content) {
        return Message("user", content);
    }

    static Message system_message(const json& content) {
        return Message("system", content);
    }

    static Message tool_message(const json& content, const std::string& tool_call_id = "", const std::string& name =  "") {
        return Message("tool", content, name, tool_call_id);
    }

    static Message assistant_message(const json& content = "", const std::vector<ToolCall>& tool_calls = {}) {
        return Message("assistant", content, "", "", tool_calls);
    }
};

struct MemoryItem {
    size_t id;              // The unique identifier for the text data
    std::string memory;     // The memory deduced from the text data
    std::string hash;       // The hash of the memory
    long long created_at;   // The creation time of the memory, 
    long long updated_at;   // The last update time of the memory
    float score;            // The score associated with the text data, used for ranking and sorting

    MemoryItem(size_t id = -1, const std::string& memory = "")
    : id(id), memory(memory) {
        hash = httplib::detail::MD5(memory);
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        created_at = now;
        updated_at = now;
        score = -1.0f;
    }

    void update_memory(const std::string& memory) {
        this->memory = memory;
        hash = httplib::detail::MD5(memory);
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        updated_at = now;
    }

    bool empty() const {
        return memory.empty();
    }
};

typedef std::function<bool(const MemoryItem&)> FilterFunc;

} // namespace humanus

#endif // HUMANUS_SCHEMA_H
