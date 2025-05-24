#ifndef HUMANUS_MEMORY_BASE_H
#define HUMANUS_MEMORY_BASE_H

#include "schema.h"
#include "memory/base.h"
#include "vector_store/base.h"
#include "embedding_model/base.h"
#include "schema.h"
#include "prompt.h"
#include "httplib.h"
#include "llm.h"
#include "utils.h"
#include "tool/fact_extract.h"
#include "tool/memory.h"
#include <deque>
#include <vector>

namespace humanus {

struct BaseMemory {
    std::deque<Message> messages;
    std::string current_request;

    // Add a message to the memory
    virtual bool add_message(const Message& message) {
        messages.push_back(message);
        return true;
    }

    // Add multiple messages to the memory
    virtual bool add_messages(const std::vector<Message>& messages) {
        for (const auto& message : messages) {
            if (!add_message(message)) {
                return false;
            }
        }
        return true;
    }
    
    // Clear all messages
    virtual void clear() {
        messages.clear();
    }

    virtual std::vector<Message> get_messages(const std::string& query = "") const {
        std::vector<Message> result;
        for (const auto& message : messages) {
            result.push_back(message);
        }
        return result;
    }

    // Convert messages to list of dicts
    json to_json_list() const {
        json memory = json::array();
        for (const auto& message : messages) {
            memory.push_back(message.to_json());
        }
        return memory;
    }
};

struct Memory : BaseMemory {
    MemoryConfig config;
    
    std::string fact_extraction_prompt;
    std::string update_memory_prompt;
    int max_messages;
    int max_token_message;
    int max_tokens_messages;
    int max_tokens_context;
    int retrieval_limit;
    FilterFunc filter;

    std::shared_ptr<EmbeddingModel> embedding_model;
    std::shared_ptr<VectorStore> vector_store;
    std::shared_ptr<LLM> llm;
    std::shared_ptr<LLM> llm_vision;

    std::shared_ptr<FactExtract> fact_extract_tool;
    std::shared_ptr<MemoryTool> memory_tool;

    bool retrieval_enabled;

    int num_tokens_messages;
    
    Memory(const MemoryConfig& config) : config(config) {
        fact_extraction_prompt = config.fact_extraction_prompt;
        update_memory_prompt = config.update_memory_prompt;
        max_messages = config.max_messages;
        max_token_message = config.max_tokens_message;
        max_tokens_messages = config.max_tokens_messages;
        max_tokens_context = config.max_tokens_context;
        retrieval_limit = config.retrieval_limit;
        filter = config.filter;

        size_t pos = fact_extraction_prompt.find("{current_date}");
        if (pos != std::string::npos) {
            // %Y-%d-%m
            auto current_date = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(current_date);
            std::stringstream ss;
            std::tm tm_info = *std::localtime(&in_time_t);
            ss << std::put_time(&tm_info, "%Y-%m-%d");
            std::string formatted_date = ss.str(); // YYYY-MM-DD
            fact_extraction_prompt.replace(pos, 14, formatted_date);
        }

        try {
            embedding_model = EmbeddingModel::get_instance(config.embedding_model, config.embedding_model_config);
            vector_store = VectorStore::get_instance(config.vector_store, config.vector_store_config);
            llm = LLM::get_instance(config.llm, config.llm_config);
            llm_vision = LLM::get_instance(config.llm_vision, config.llm_vision_config);
            
            logger->info("🔥 Memory is warming up...");
            auto test_response = llm->ask(
                {Message::user_message("Hello")}
            );
            auto test_embedding = embedding_model->embed(test_response, EmbeddingType::ADD);
            vector_store->insert(test_embedding, 0);
            vector_store->remove(0);
            logger->info("📒 Memory is ready!");

            retrieval_enabled = true;
        } catch (const std::exception& e) {
            logger->warn("Error in initializing memory: " + std::string(e.what()) + ", fallback to default FIFO memory");
            embedding_model = nullptr;
            vector_store = nullptr;
            llm = nullptr;
            llm_vision = nullptr;
            retrieval_enabled = false;
        }

        if (llm_vision && llm_vision->enable_vision() == false) { // Make sure it can handle vision messages
            llm_vision = nullptr;
        }

        fact_extract_tool = std::make_shared<FactExtract>();
        memory_tool = std::make_shared<MemoryTool>();
    }

    bool add_message(const Message& message) override {
        if (message.num_tokens > config.max_tokens_message) {
            logger->warn("Message is too long, skipping"); // TODO: use content_provider to handle this
            return false; 
        }
        messages.push_back(message);
        num_tokens_messages += message.num_tokens;
        std::vector<Message> messages_to_memory;
        while (messages.size() > max_messages || num_tokens_messages > config.max_tokens_messages) {
            messages_to_memory.push_back(messages.front());
            num_tokens_messages -= messages.front().num_tokens;
            messages.pop_front();
        }
        if (!messages.empty()) { // Ensure the first message is always a user or system message
            if (messages.front().role == "assistant") {
                messages.push_front(Message::user_message("Current request: " + current_request + "\n\nDue to limited memory, some previous messages are not shown."));
                num_tokens_messages += messages.front().num_tokens;
            } else if (messages.front().role == "tool") {
                messages_to_memory.push_back(messages.front());
                num_tokens_messages -= messages.front().num_tokens;
                messages.pop_front();
            }
        }
        if (retrieval_enabled && !messages_to_memory.empty()) {
            if (llm_vision) { // TODO: configure to use multimodal embedding model instead of converting to text
                for (auto& m : messages_to_memory) {
                    m = parse_vision_message(m, llm_vision, llm_vision->vision_details());
                }
            } else { // Convert to a padding message indicating that the message is a vision message (but not description)
                for (auto& m : messages_to_memory) {
                    m = parse_vision_message(m);
                }
            }
            _add_to_vector_store(messages_to_memory); 
        }
        return true;
    }

    std::vector<Message> get_messages(const std::string& query = "") const override {
        std::vector<Message> messages_with_memory;

        if (retrieval_enabled && !query.empty()) {
            auto embeddings = embedding_model->embed(
                query,
                EmbeddingType::SEARCH
            );
            std::vector<MemoryItem> memories;
            
            // Check if vectore store is available
            if (vector_store) {
                memories = vector_store->search(embeddings, retrieval_limit, filter);
            }

            if (!memories.empty()) {
                sort(memories.begin(), memories.end(), [](const MemoryItem& a, const MemoryItem& b) {
                    return a.updated_at > b.updated_at;
                });

                int num_tokens_context = num_tokens_messages;
                std::deque<Message> memory_messages;

                for (const auto& memory_item : memories) { // Make sure the oldest memory is at the front of the deque and the tokens within the limit
                    auto memory_message = Message::user_message("<memory>" + memory_item.memory + "</memory>");
                    if (num_tokens_context + memory_message.num_tokens > config.max_tokens_context) {
                        break;
                    }
                    num_tokens_context += memory_message.num_tokens;
                    memory_messages.push_front(memory_message);
                }

                logger->info("📤 Total retreived memories: " + std::to_string(memory_messages.size()));

                messages_with_memory.insert(messages_with_memory.end(), memory_messages.begin(), memory_messages.end());
            }
        }

        messages_with_memory.insert(messages_with_memory.end(), messages.begin(), messages.end());

        return messages_with_memory;
    }

    void clear() override {
        if (messages.empty()) {
            return;
        }
        if (retrieval_enabled) {
            std::vector<Message> messages_to_memory(messages.begin(), messages.end());
            _add_to_vector_store(messages_to_memory);
        }
        messages.clear();
    }

    void _add_to_vector_store(const std::vector<Message>& messages) {
        // Check if vector store is available
        if (!vector_store) {
            logger->warn("Vector store is not initialized, skipping memory operation");
            return;
        }
        
        std::string parsed_message;
        
        for (const auto& message : messages) {
            parsed_message += message.role + ": " + (message.content.is_string() ? message.content.get<std::string>() : message.content.dump()) + "\n";

            for (const auto& tool_call : message.tool_calls) {
                parsed_message += "<tool_call>" + tool_call.to_json().dump() + "</tool_call>\n";
            }
        }
    
        std::string system_prompt = fact_extraction_prompt;

        size_t pos = system_prompt.find("{current_request}");
        if (pos != std::string::npos) {
            system_prompt = system_prompt.replace(pos, 17, current_request);
        }

        std::string user_prompt = "<input>" + parsed_message + "</input>";

        Message user_message = Message::user_message(user_prompt);

        json response = llm->ask_tool(
            {user_message},
            system_prompt,
            "",
            json::array({fact_extract_tool->to_param()}),
            "required"
        );

        std::vector<std::string> new_facts; // ["fact1", "fact2", "fact3"]

        try {
            auto tool_calls = ToolCall::from_json_list(response["tool_calls"]);
            for (const auto& tool_call : tool_calls) {
                if (tool_call.function.name != "fact_extract") { // might be other tools because of hallucinations (e.g. wrongly responsed to user message)
                    continue;
                }
                // Parse arguments
                json args = tool_call.function.arguments;

                if (args.is_string()) {
                    args = json::parse(args.get<std::string>());
                }

                auto facts = fact_extract_tool->execute(args).output.get<std::vector<std::string>>();
                if (!facts.empty()) {
                    new_facts.insert(new_facts.end(), facts.begin(), facts.end());
                }
            }
        } catch (const std::exception& e) {
            logger->warn("Error in new_facts: " + std::string(e.what()));
        }

        if (new_facts.empty()) {
            return;
        }

        logger->info("📫 New facts to remember: " + json(new_facts).dump());

        std::vector<json> old_memories;
        std::map<std::string, std::vector<float>> new_message_embeddings;

        for (const auto& fact : new_facts) {
            auto message_embedding = embedding_model->embed(fact, EmbeddingType::ADD);
            new_message_embeddings[fact] = message_embedding;
            auto existing_memories = vector_store->search(
                message_embedding,
                5
            );
            for (const auto& memory : existing_memories) {
                old_memories.push_back({
                    {"id", memory.id},
                    {"text", memory.memory}
                });
            }
        }
        // sort and unique by id
        std::sort(old_memories.begin(), old_memories.end(), [](const json& a, const json& b) {
            return a["id"] < b["id"];
        });
        old_memories.resize(std::unique(old_memories.begin(), old_memories.end(), [](const json& a, const json& b) {
            return a["id"] == b["id"];
        }) - old_memories.begin());
        logger->info("📒 Existing memories about new facts: " + std::to_string(old_memories.size()));

        // mapping UUIDs with integers for handling ID hallucinations
        std::vector<size_t> temp_id_mapping;
        for (size_t idx = 0; idx < old_memories.size(); ++idx) {
            temp_id_mapping.push_back(old_memories[idx]["id"].get<size_t>());
            old_memories[idx]["id"] = idx;
        }

        std::string function_calling_prompt = get_update_memory_messages(old_memories, new_facts, update_memory_prompt);

        // std::string new_memories_with_actions_str;
        // json new_memories_with_actions = json::array();

        // try {
        //     new_memories_with_actions_str = llm->ask(
        //         {Message::user_message(function_calling_prompt)}
        //     );
        //     new_memories_with_actions_str = remove_code_blocks(new_memories_with_actions_str);
        // } catch (const std::exception& e) {
        //     logger->error("Error in parsing new_memories_with_actions: " + std::string(e.what()));
        // }
        
        // try {
        //     new_memories_with_actions = json::parse(new_memories_with_actions_str);
        // } catch (const std::exception& e) {
        //     logger->error("Invalid JSON response: " + std::string(e.what()));
        // }

        response = llm->ask_tool(
            {Message::user_message(function_calling_prompt)},
            "", // system prompt
            "", // next step prompt
            json::array({memory_tool->to_param()}),
            "required"
        );

        std::vector<json> memory_events;

        try {
            auto tool_calls = ToolCall::from_json_list(response["tool_calls"]);
            for (const auto& tool_call : tool_calls) {
                if (tool_call.function.name != "memory") { // might be other tools because of hallucinations (e.g. wrongly responsed to user message)
                    continue;
                }
                // Parse arguments
                json args = tool_call.function.arguments;

                if (args.is_string()) {
                    args = json::parse(args.get<std::string>());
                }

                auto events = memory_tool->execute(args).output.get<std::vector<json>>();
                if (!events.empty()) {
                    memory_events.insert(memory_events.end(), events.begin(), events.end());
                }
            }
        } catch (const std::exception& e) {
            logger->warn("Error in memory_events: " + std::string(e.what()));
        }

        try {
            for (const auto& event : memory_events) {
                logger->debug("Processing memory: " + event.dump(2));
                try {
                    if (!event.contains("text")) {
                        logger->warn("Skipping memory entry because of empty `text` field.");
                        continue;
                    }
                    std::string type = event.value("type", "NONE");
                    size_t memory_id;
                    try {
                        if (type != "ADD") {
                            memory_id = temp_id_mapping.at(event["id"].get<size_t>());
                        } else {
                            memory_id = get_uuid_64();
                        }
                    } catch (...) {
                        memory_id = get_uuid_64();
                    }
                    if (type == "ADD") {
                        _create_memory(
                            memory_id,
                            event["text"], // data
                            new_message_embeddings // existing_embeddings
                        );
                    } else if (type == "UPDATE") {
                        _update_memory(
                            memory_id,
                            event["text"], // data
                            new_message_embeddings // existing_embeddings
                        );
                    } else if (type == "DELETE") {
                        _delete_memory(memory_id);
                    }
                } catch (const std::exception& e) {
                    logger->error("Error in new_memories_with_actions: " + std::string(e.what()));
                }
            }
        } catch (const std::exception& e) {
            logger->error("Error in new_memories_with_actions: " + std::string(e.what()));
        }
    }

    void _create_memory(const size_t& memory_id, const std::string& data, const std::map<std::string, std::vector<float>>& existing_embeddings) {
        if (!vector_store) {
            logger->warn("Vector store is not initialized, skipping create memory");
            return;
        }

        logger->info("🆕 Creating memory: " + data);
        
        std::vector<float> embedding;
        if (existing_embeddings.find(data) != existing_embeddings.end()) {
            embedding = existing_embeddings.at(data);
        } else {
            embedding = embedding_model->embed(data, EmbeddingType::ADD);
        }

        MemoryItem metadata{
            memory_id,
            data
        };

        vector_store->insert(
            embedding,
            memory_id,
            metadata
        );
    }

    void _update_memory(const size_t& memory_id, const std::string& data, const std::map<std::string, std::vector<float>>& existing_embeddings) {
        if (!vector_store) {
            logger->warn("Vector store is not initialized, skipping update memory");
            return;
        }
        
        MemoryItem existing_memory;

        try {
            existing_memory = vector_store->get(memory_id);
        } catch (const std::exception& e) {
            logger->error("Error fetching existing memory: " + std::string(e.what()));
            return;
        }
        
        logger->info("🆕 Updating memory: (old) " + existing_memory.memory + " (new) " + data);

        std::vector<float> embedding;
        if (existing_embeddings.find(data) != existing_embeddings.end()) {
            embedding = existing_embeddings.at(data);
        } else {
            embedding = embedding_model->embed(data, EmbeddingType::ADD);
        }

        existing_memory.update_memory(data);

        vector_store->update(
            memory_id,
            embedding,
            existing_memory
        );
    }

    void _delete_memory(const size_t& memory_id) {
        if (!vector_store) {
            logger->warn("Vector store is not initialized, skipping delete memory");
            return;
        }
        
        logger->info("❌ Deleting memory: " + std::to_string(memory_id));
        vector_store->remove(memory_id);
    }
};

}

#endif // HUMANUS_MEMORY_BASE_H