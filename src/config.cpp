#include "config.h"

namespace humanus {

LLMConfig LLMConfig::load_from_toml(const toml::table& config_table) {
    LLMConfig config;

    try {
        if (config_table.contains("model")) {
            config.model = config_table["model"].as_string()->get();
        }

        if (config_table.contains("api_key")) {
            config.api_key = config_table["api_key"].as_string()->get();
        }

        if (config_table.contains("base_url")) {
            config.base_url = config_table["base_url"].as_string()->get();
        }

        if (config_table.contains("endpoint")) {
            config.endpoint = config_table["endpoint"].as_string()->get();
        }

        if (config_table.contains("vision_details")) {
            config.vision_details = config_table["vision_details"].as_string()->get();
        }
        
        if (config_table.contains("max_tokens")) {
            config.max_tokens = config_table["max_tokens"].as_integer()->get();
        }

        if (config_table.contains("timeout")) {
            config.timeout = config_table["timeout"].as_integer()->get();
        }

        if (config_table.contains("temperature")) {
            config.temperature = config_table["temperature"].as_floating_point()->get();
        }

        if (config_table.contains("enable_vision")) {
            config.enable_vision = config_table["enable_vision"].as_boolean()->get();
        }

        if (config_table.contains("enable_tool")) {
            config.enable_tool = config_table["enable_tool"].as_boolean()->get();
        }

        if (config_table.contains("enable_thinking")) {
            config.enable_thinking = config_table["enable_thinking"].as_boolean()->get();
        }

        if (!config.enable_tool) {
            // Load tool parser configuration
            ToolParser tool_parser;
            if (config_table.contains("tool_start")) {
                tool_parser.tool_start = config_table["tool_start"].as_string()->get();
            }

            if (config_table.contains("tool_end")) {
                tool_parser.tool_end = config_table["tool_end"].as_string()->get();
            }

            if (config_table.contains("tool_hint_template")) {
                tool_parser.tool_hint_template = config_table["tool_hint_template"].as_string()->get();
            }
            config.tool_parser = tool_parser;
        }

        return config;
    } catch (const std::exception& e) {
        logger->error("Failed to load LLM configuration: " + std::string(e.what()));
        throw;
    }

    return config;
}

MCPServerConfig MCPServerConfig::load_from_toml(const toml::table& config_table) {
    MCPServerConfig config;
    
    try {
        // Read type
        if (!config_table.contains("type") || !config_table["type"].is_string()) {
            throw std::runtime_error("Tool configuration missing type field, expected sse or stdio.");
        }

        config.type = config_table["type"].as_string()->get();

        if (config.type == "stdio") {
            // Read command
            if (!config_table.contains("command") || !config_table["command"].is_string()) {
                throw std::runtime_error("stdio type tool configuration missing command field.");
            }
            config.command = config_table["command"].as_string()->get();
            
            // Read arguments (if any)
            if (config_table.contains("args")) {
                const auto& args_array = *config_table["args"].as_array();
                for (const auto& arg : args_array) {
                    if (arg.is_string()) {
                        config.args.push_back(arg.as_string()->get());
                    }
                }
            }
            
            // Read environment variables
            if (config_table.contains("env")) {
                const auto& env_table = *config_table["env"].as_table();
                for (const auto& [key, value] : env_table) {
                    if (value.is_string()) {
                        config.env_vars[key] = value.as_string()->get();
                    } else if (value.is_integer()) {
                        config.env_vars[key] = value.as_integer()->get();
                    } else if (value.is_floating_point()) {
                        config.env_vars[key] = value.as_floating_point()->get();
                    } else if (value.is_boolean()) {
                        config.env_vars[key] = value.as_boolean()->get();
                    }
                }
            }
        } else if (config.type == "sse") {
            // Read host and port or url
            if (config_table.contains("url")) {
                config.url = config_table["url"].as_string()->get();
            } else {
                if (!config_table.contains("host")) {
                    throw std::runtime_error("sse type tool configuration missing host field");
                }
                config.host = config_table["host"].as_string()->get();

                if (!config_table.contains("port")) {
                    throw std::runtime_error("sse type tool configuration missing port field");
                }
                config.port = config_table["port"].as_integer()->get();
            }
        } else {
            throw std::runtime_error("Unsupported tool type: " + config.type);
        }
    } catch (const std::exception& e) {
        logger->error("Failed to load MCP tool configuration: " + std::string(e.what()));
        throw;
    }
    
    return config;
}

EmbeddingModelConfig EmbeddingModelConfig::load_from_toml(const toml::table& config_table) {
    EmbeddingModelConfig config;

    try {
        if (config_table.contains("provider")) {
            config.provider = config_table["provider"].as_string()->get();
        }

        if (config_table.contains("base_url")) {
            config.base_url = config_table["base_url"].as_string()->get();
        }

        if (config_table.contains("endpoint")) {
            config.endpoint = config_table["endpoint"].as_string()->get();
        }

        if (config_table.contains("model")) {
            config.model = config_table["model"].as_string()->get();
        }

        if (config_table.contains("api_key")) {
            config.api_key = config_table["api_key"].as_string()->get();
        }

        if (config_table.contains("embedding_dims")) {
            config.embedding_dims = config_table["embedding_dims"].as_integer()->get();
        }

        if (config_table.contains("max_retries")) {
            config.max_retries = config_table["max_retries"].as_integer()->get();
        }
    } catch (const std::exception& e) {
        logger->error("Failed to load embedding model configuration: " + std::string(e.what()));
        throw;
    }
    
    return config;
}

VectorStoreConfig VectorStoreConfig::load_from_toml(const toml::table& config_table) {
    VectorStoreConfig config;

    try {
        if (config_table.contains("provider")) {
            config.provider = config_table["provider"].as_string()->get();
        }

        if (config_table.contains("dim")) {
            config.dim = config_table["dim"].as_integer()->get();
        }

        if (config_table.contains("max_elements")) {
            config.max_elements = config_table["max_elements"].as_integer()->get();
        }

        if (config_table.contains("M")) {
            config.M = config_table["M"].as_integer()->get();
        }

        if (config_table.contains("ef_construction")) {
            config.ef_construction = config_table["ef_construction"].as_integer()->get();
        }

        if (config_table.contains("metric")) {
            const auto& metric_str = config_table["metric"].as_string()->get();
            if (metric_str == "L2") {
                config.metric = VectorStoreConfig::Metric::L2;
            } else if (metric_str == "IP") {
                config.metric = VectorStoreConfig::Metric::IP;
            } else {
                throw std::runtime_error("Invalid metric: " + metric_str);
            }
        }
    } catch (const std::exception& e) {
        logger->error("Failed to load vector store configuration: " + std::string(e.what()));
        throw;
    }
    
    return config;
}

MemoryConfig MemoryConfig::load_from_toml(const toml::table& config_table) {
    MemoryConfig config;

    try {
        // Base config
        if (config_table.contains("max_messages")) {
            config.max_messages = config_table["max_messages"].as_integer()->get();
        }

        if (config_table.contains("max_tokens_message")) {
            config.max_tokens_message = config_table["max_tokens_message"].as_integer()->get();
        }

        if (config_table.contains("max_tokens_messages")) {
            config.max_tokens_messages = config_table["max_tokens_messages"].as_integer()->get();
        }

        if (config_table.contains("max_tokens_context")) {
            config.max_tokens_context = config_table["max_tokens_context"].as_integer()->get();
        }

        if (config_table.contains("retrieval_limit")) {
            config.retrieval_limit = config_table["retrieval_limit"].as_integer()->get();
        }

        // Prompt config
        if (config_table.contains("fact_extraction_prompt")) {
            config.fact_extraction_prompt = config_table["fact_extraction_prompt"].as_string()->get();
        }

        if (config_table.contains("update_memory_prompt")) {
            config.update_memory_prompt = config_table["update_memory_prompt"].as_string()->get();
        }
        
        // EmbeddingModel config
        if (config_table.contains("embedding_model")) {
            config.embedding_model = config_table["embedding_model"].as_string()->get();
        }

        // Vector store config
        if (config_table.contains("vector_store")) {
            config.vector_store = config_table["vector_store"].as_string()->get();
        }

        // LLM config
        if (config_table.contains("llm")) {
            config.llm = config_table["llm"].as_string()->get();
        }

        if (config_table.contains("llm_vision")) {
            config.llm_vision = config_table["llm_vision"].as_string()->get();
        }
    } catch (const std::exception& e) {
        logger->error("Failed to load memory configuration: " + std::string(e.what()));
        throw;
    }

    return config;
}

// Initialize static members
std::shared_mutex Config::_config_mutex;

void Config::_load_llm_config() {
    std::unique_lock<std::shared_mutex> lock(_config_mutex);
    try {
        auto config_path = _get_llm_config_path();
        logger->info("Loading LLM config file from: " + config_path.string());
        
        const auto& data = toml::parse_file(config_path.string());

        // Load LLM configuration
        for (const auto& [key, value] : data) {
            const auto& config_table = *value.as_table();
            logger->info("Loading LLM config: " + std::string(key.str()));
            auto config = LLMConfig::load_from_toml(config_table);
            llm[std::string(key.str())] = config;
            if (config.enable_vision && llm.find("vision_default") == llm.end()) {
                llm["vision_default"] = config;
            }
        }

        if (llm.empty()) {
            throw std::runtime_error("No LLM configuration found");
        } else if (llm.find("default") == llm.end()) {
            llm["default"] = llm.begin()->second;
        }
    } catch (const std::exception& e) {
        logger->error("Failed to load LLM configuration: " + std::string(e.what()));
        throw;
    }
}

void Config::_load_mcp_server_config() {
    std::unique_lock<std::shared_mutex> lock(_config_mutex);
    try {
        auto config_path = _get_mcp_server_config_path();
        logger->info("Loading MCP server config file from: " + config_path.string());

        const auto& data = toml::parse_file(config_path.string());

        // Load MCP server configuration
        for (const auto& [key, value] : data) {
            const auto& config_table = *value.as_table();
            logger->info("Loading MCP server config: " + std::string(key.str()));
            mcp_server[std::string(key.str())] = MCPServerConfig::load_from_toml(config_table);
        }
    } catch (const std::exception& e) {
        logger->warn("Failed to load MCP server configuration: " + std::string(e.what()));
    }
}

void Config::_load_memory_config() {
    std::unique_lock<std::shared_mutex> lock(_config_mutex);
    try {
        auto config_path = _get_memory_config_path();
        logger->info("Loading memory config file from: " + config_path.string());

        const auto& data = toml::parse_file(config_path.string());

        // Load memory configuration
        for (const auto& [key, value] : data) {
            const auto& config_table = *value.as_table();
            logger->info("Loading memory config: " + std::string(key.str()));
            memory[std::string(key.str())] = MemoryConfig::load_from_toml(config_table);
        }
    } catch (const std::exception& e) {
        logger->warn("Failed to load memory configuration: " + std::string(e.what()));
    }
}

void Config::_load_embedding_model_config() {
    std::unique_lock<std::shared_mutex> lock(_config_mutex);
    try {
        auto config_path = _get_embedding_model_config_path();
        logger->info("Loading embedding model config file from: " + config_path.string());
        
        const auto& data = toml::parse_file(config_path.string());

        // Load embedding model configuration
        for (const auto& [key, value] : data) {
            const auto& config_table = *value.as_table();
            logger->info("Loading embedding model config: " + std::string(key.str()));
            embedding_model[std::string(key.str())] = EmbeddingModelConfig::load_from_toml(config_table);
        }

        if (embedding_model.empty()) {
            throw std::runtime_error("No embedding model configuration found");
        } else if (embedding_model.find("default") == embedding_model.end()) {
            embedding_model["default"] = embedding_model.begin()->second;
        }
    } catch (const std::exception& e) {
        logger->warn("Failed to load embedding model configuration: " + std::string(e.what()));
        // Set default configuration
        embedding_model["default"] = EmbeddingModelConfig();
    }
}

void Config::_load_vector_store_config() {
    std::unique_lock<std::shared_mutex> lock(_config_mutex);
    try {
        auto config_path = _get_vector_store_config_path();
        logger->info("Loading vector store config file from: " + config_path.string());
        
        const auto& data = toml::parse_file(config_path.string());

        // Load vector store configuration
        for (const auto& [key, value] : data) {
            const auto& config_table = *value.as_table();
            logger->info("Loading vector store config: " + std::string(key.str()));
            vector_store[std::string(key.str())] = VectorStoreConfig::load_from_toml(config_table);
        }

        if (vector_store.empty()) {
            throw std::runtime_error("No vector store configuration found");
        } else if (vector_store.find("default") == vector_store.end()) {
            vector_store["default"] = vector_store.begin()->second;
        }
    } catch (const std::exception& e) {
        logger->warn("Failed to load vector store configuration: " + std::string(e.what()));
        // Set default configuration
        vector_store["default"] = VectorStoreConfig();
    }
}

LLMConfig Config::get_llm_config(const std::string& config_name) {
    auto& instance = get_instance();
    std::shared_lock<std::shared_mutex> lock(_config_mutex);
    
    bool need_default = instance.llm.find(config_name) == instance.llm.end();
    if (need_default) {
        std::string message = "LLM config not found: " + config_name + ", falling back to default LLM config.";
        lock.unlock();
        logger->warn(message);
        lock.lock();
        return instance.llm.at("default");
    } else {
        return instance.llm.at(config_name);
    }
}

MCPServerConfig Config::get_mcp_server_config(const std::string& config_name) {
    auto& instance = get_instance();
    std::shared_lock<std::shared_mutex> lock(_config_mutex);
    return instance.mcp_server.at(config_name);
}

MemoryConfig Config::get_memory_config(const std::string& config_name) {
    auto& instance = get_instance();
    std::shared_lock<std::shared_mutex> lock(_config_mutex);
    
    bool need_default = instance.memory.find(config_name) == instance.memory.end();
    if (need_default) {
        std::string message = "Memory config not found: " + config_name + ", falling back to default memory config.";
        lock.unlock();
        logger->warn(message);
        lock.lock();
        return instance.memory.at("default");
    } else {
        return instance.memory.at(config_name);
    }
}

EmbeddingModelConfig Config::get_embedding_model_config(const std::string& config_name) {
    auto& instance = get_instance();
    std::shared_lock<std::shared_mutex> lock(_config_mutex);
    
    bool need_default = instance.embedding_model.find(config_name) == instance.embedding_model.end();
    if (need_default) {
        std::string message = "Embedding model config not found: " + config_name + ", falling back to default embedding model config.";
        lock.unlock();
        logger->warn(message);
        lock.lock();
        return instance.embedding_model.at("default");
    } else {
        return instance.embedding_model.at(config_name);
    }
}

VectorStoreConfig Config::get_vector_store_config(const std::string& config_name) {
    auto& instance = get_instance();
    std::shared_lock<std::shared_mutex> lock(_config_mutex);
    
    bool need_default = instance.vector_store.find(config_name) == instance.vector_store.end();
    if (need_default) {
        std::string message = "Vector store config not found: " + config_name + ", falling back to default vector store config.";
        lock.unlock();
        logger->warn(message);
        lock.lock();
        return instance.vector_store.at("default");
    } else {
        return instance.vector_store.at(config_name);
    }
}

} // namespace humanus