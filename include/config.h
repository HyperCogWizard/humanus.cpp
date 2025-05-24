#ifndef HUMANUS_CONFIG_H
#define HUMANUS_CONFIG_H

#include "schema.h"
#include "prompt.h"
#include "logger.h"
#include "toml.hpp"
#include "utils.h"
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace humanus {

struct ToolParser {
    std::string tool_start;
    std::string tool_end;
    std::string tool_hint_template;

    ToolParser(const std::string& tool_start = "<tool_call>", const std::string& tool_end = "</tool_call>", const std::string& tool_hint_template = prompt::toolcall::TOOL_HINT_TEMPLATE)
        : tool_start(tool_start), tool_end(tool_end), tool_hint_template(tool_hint_template) {}

    static std::string str_replace(std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
        }
        return str;
    }

    std::string hint(const std::string& tool_list) const {
        std::string hint_str = tool_hint_template;
        hint_str = str_replace(hint_str, "{tool_start}", tool_start);
        hint_str = str_replace(hint_str, "{tool_end}", tool_end);
        hint_str = str_replace(hint_str, "{tool_list}", tool_list);
        return hint_str;
    }

    json parse(const std::string& content) const {
        std::string new_content = content;
        json tool_calls = json::array();

        size_t pos_start = new_content.find(tool_start);
        size_t pos_end = pos_start == std::string::npos ? std::string::npos : new_content.find(tool_end, pos_start + tool_start.size());

        if (pos_start != std::string::npos && pos_end == std::string::npos) { // Some might not have tool_end
            pos_end = new_content.size();
        }

        while (pos_start != std::string::npos) {
            std::string tool_content = new_content.substr(pos_start + tool_start.size(), pos_end - pos_start - tool_start.size());
            
            if (!tool_content.empty()) {
                try {
                    tool_calls.push_back({
                        {"type", "function"},
                        {"function", json::parse(tool_content)}
                    });
                    tool_calls.back()["id"] = "call_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                } catch (const json::exception& /* e */) {
                    throw std::runtime_error("Invalid tool call: " + tool_content);
                }
            }

            auto trim = [](const std::string& str) -> std::string {
                auto not_space = [](unsigned char ch) { return !std::isspace(ch); };

                auto start = std::find_if(str.begin(), str.end(), not_space);
                auto end = std::find_if(str.rbegin(), str.rend(), not_space).base();

                if (start >= end) return "";
                return std::string(start, end);
            };

            std::string lhs = trim(new_content.substr(0, pos_start));
            std::string rhs = trim(new_content.substr(std::min(pos_end + tool_end.size(), new_content.size())));

            new_content = lhs + rhs;

            pos_start = new_content.find(tool_start, pos_start); // Previous tool_call has been cut off
            pos_end = pos_start == std::string::npos ? std::string::npos : new_content.find(tool_end, pos_start + tool_start.size());
            if (pos_start != std::string::npos && pos_end == std::string::npos) { // Some might not have tool_end
                pos_end = new_content.size();
            }
        }

        return {
            {"content", new_content},
            {"tool_calls", tool_calls} // Might be empty if no tool calls found
        };
    }

    json dump(const json& tool_calls) const {
        std::string content;
        if (!tool_calls.is_array()) {
            throw std::runtime_error("Tool calls should be an array");
        }
        for (const auto& tool_call : tool_calls) {
            content += tool_start;
            content += tool_call[tool_call["type"]].dump(2);
            content += tool_end;
        }
        return content;
    }
};

struct LLMConfig {
    std::string model;
    std::string api_key;
    std::string base_url;
    std::string endpoint;
    std::string vision_details;
    int max_tokens;
    int timeout;
    double temperature;
    bool enable_vision;
    bool enable_tool;
    bool enable_thinking; // Qwen3 thinking settings (must be set to false for non-streaming calls)

    ToolParser tool_parser;

    LLMConfig(
        const std::string& model = "deepseek-chat", 
        const std::string& api_key = "sk-", 
        const std::string& base_url = "https://api.deepseek.com", 
        const std::string& endpoint = "/v1/chat/completions",
        const std::string& vision_details = "auto",
        int max_tokens = -1, // -1 for default
        int timeout = 120, 
        double temperature = -1, // -1 for default
        bool enable_vision = false,
        bool enable_tool = true,
        bool enable_thinking = false,
        const ToolParser& tool_parser = ToolParser()
    ) : model(model), api_key(api_key), base_url(base_url), endpoint(endpoint), vision_details(vision_details),
        max_tokens(max_tokens), timeout(timeout), temperature(temperature), enable_vision(enable_vision), enable_tool(enable_tool), enable_thinking(enable_thinking), tool_parser(tool_parser) {}
        
    static LLMConfig load_from_toml(const toml::table& config_table);
};

// Read tool configuration from config_mcp.toml
struct MCPServerConfig {
    std::string type;
    std::string host;
    int port;
    std::string url;
    std::string command;
    std::vector<std::string> args;
    json env_vars = json::object();
    
    static MCPServerConfig load_from_toml(const toml::table& config_table);
};

enum class EmbeddingType {
    ADD = 0,
    SEARCH = 1,
    UPDATE = 2
};

struct EmbeddingModelConfig {
    std::string provider = "oai";
    std::string base_url = "http://localhost:8080";
    std::string endpoint = "/v1/embeddings";
    std::string model = "nomic-embed-text-v1.5.f16.gguf";
    std::string api_key = "";
    int embedding_dims = 768;
    int max_retries = 3;

    static EmbeddingModelConfig load_from_toml(const toml::table& config_table);
};

struct VectorStoreConfig {
    std::string provider = "hnswlib";
    int dim = 16;                    // Dimension of the elements
    int max_elements = 10000;        // Maximum number of elements, should be known beforehand
    int M = 16;                      // Tightly connected with internal dimensionality of the data
                                     // strongly affects the memory consumption
    int ef_construction = 200;       // Controls index search speed/build speed tradeoff
    enum class Metric {
        L2,
        IP
    };
    Metric metric = Metric::L2;

    static VectorStoreConfig load_from_toml(const toml::table& config_table);
};

struct MemoryConfig {
    // Base config
    int max_messages = 16;                  // Maximum number of messages in short-term memory
    int max_tokens_message = 1 << 15;       // Maximum number of tokens in single message
    int max_tokens_messages = 1 << 16;      // Maximum number of tokens in short-term memory
    int max_tokens_context = 1 << 17;       // Maximum number of tokens in context (used by `get_messages`)
    int retrieval_limit = 32;               // Maximum number of results to retrive from long-term memory

    // Prompt config
    std::string fact_extraction_prompt = prompt::FACT_EXTRACTION_PROMPT;
    std::string update_memory_prompt = prompt::UPDATE_MEMORY_PROMPT;
    
    // EmbeddingModel config
    std::string embedding_model = "default";
    std::shared_ptr<EmbeddingModelConfig> embedding_model_config = nullptr;
    
    // Vector store config
    std::string vector_store = "default";
    std::shared_ptr<VectorStoreConfig> vector_store_config = nullptr;
    FilterFunc filter = nullptr;  // Filter to apply to search results
    
    // LLM config
    std::string llm = "default";
    std::shared_ptr<LLMConfig> llm_config = nullptr;
    std::string llm_vision = "vision_default";
    std::shared_ptr<LLMConfig> llm_vision_config = nullptr;

    static MemoryConfig load_from_toml(const toml::table& config_table);
};

class Config {
private:
    static std::shared_mutex _config_mutex;
    bool _initialized = false;
    std::unordered_map<std::string, LLMConfig> llm;
    std::unordered_map<std::string, MCPServerConfig> mcp_server;
    std::unordered_map<std::string, MemoryConfig> memory;
    std::unordered_map<std::string, EmbeddingModelConfig> embedding_model;
    std::unordered_map<std::string, VectorStoreConfig> vector_store;
    
    Config() {
        _load_llm_config();
        _load_mcp_server_config();
        _load_memory_config();
        _load_embedding_model_config();
        _load_vector_store_config();
        _initialized = true;
    }
    
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    static std::filesystem::path _get_llm_config_path() {
        auto root = PROJECT_ROOT;
        auto config_path = root / "config" / "config_llm.toml";
        if (std::filesystem::exists(config_path)) {
            return config_path;
        }
        throw std::runtime_error("LLM Config file not found");
    }

    static std::filesystem::path _get_mcp_server_config_path() {
        auto root = PROJECT_ROOT;
        auto config_path = root / "config" / "config_mcp.toml";
        if (std::filesystem::exists(config_path)) {
            return config_path;
        }
        throw std::runtime_error("MCP Tool Config file not found");
    }

    static std::filesystem::path _get_memory_config_path() {
        auto root = PROJECT_ROOT;
        auto config_path = root / "config" / "config_mem.toml";
        if (std::filesystem::exists(config_path)) {
            return config_path;
        }
        throw std::runtime_error("Memory Config file not found");
    }

    static std::filesystem::path _get_embedding_model_config_path() {
        auto root = PROJECT_ROOT;
        auto config_path = root / "config" / "config_embd.toml";
        if (std::filesystem::exists(config_path)) {
            return config_path;
        }
        throw std::runtime_error("Embedding Model Config file not found");
    }
    
    static std::filesystem::path _get_vector_store_config_path() {
        auto root = PROJECT_ROOT;
        auto config_path = root / "config" / "config_vec.toml";
        if (std::filesystem::exists(config_path)) {
            return config_path;
        }
        throw std::runtime_error("Vector Store Config file not found");
    }
    
    void _load_llm_config();

    void _load_mcp_server_config();

    void _load_memory_config();

    void _load_embedding_model_config();

    void _load_vector_store_config();

public:
    /**
     * @brief Get the singleton instance
     * @return The config instance
     */
    static Config& get_instance() {
        static Config instance;
        return instance;
    }
    
    static LLMConfig get_llm_config(const std::string& config_name);

    static MCPServerConfig get_mcp_server_config(const std::string& config_name);

    static MemoryConfig get_memory_config(const std::string& config_name);

    static EmbeddingModelConfig get_embedding_model_config(const std::string& config_name);

    static VectorStoreConfig get_vector_store_config(const std::string& config_name);
};

} // namespace humanus

#endif // HUMANUS_CONFIG_H 