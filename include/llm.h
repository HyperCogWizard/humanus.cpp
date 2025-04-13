#ifndef HUMANUS_LLM_H
#define HUMANUS_LLM_H

#include "config.h"
#include "logger.h"
#include "schema.h"
#include "httplib.h"
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <stdexcept>
#include <future>

namespace humanus {

class LLM {
private:
    static std::unordered_map<std::string, std::shared_ptr<LLM>> instances_;

    std::unique_ptr<httplib::Client> client_;

    std::shared_ptr<LLMConfig> llm_config_;

    size_t total_prompt_tokens_;
    size_t total_completion_tokens_;
    
public:
    // Constructor
    LLM(const std::string& config_name, const std::shared_ptr<LLMConfig>& config = nullptr) : llm_config_(config) {
        client_ = std::make_unique<httplib::Client>(llm_config_->base_url);
        client_->set_default_headers({
            {"Authorization", "Bearer " + llm_config_->api_key}
        });
        client_->set_read_timeout(llm_config_->timeout);
        total_prompt_tokens_ = 0;
        total_completion_tokens_ = 0;
    }

    // Get the singleton instance
    static std::shared_ptr<LLM> get_instance(const std::string& config_name = "default", const std::shared_ptr<LLMConfig>& llm_config = nullptr) {
        if (instances_.find(config_name) == instances_.end()) {
            auto llm_config_ = llm_config;
            if (!llm_config_) {
                llm_config_ = std::make_shared<LLMConfig>(Config::get_llm_config(config_name));
            }
            instances_[config_name] = std::make_shared<LLM>(config_name, llm_config_);
        }
        return instances_[config_name];
    }

    bool enable_vision() const {
        return llm_config_->enable_vision;
    }

    std::string vision_details() const {
        return llm_config_->vision_details;
    }
    
    /**
     * @brief Format the message list to the format that LLM can accept
     * @param messages Message object message list
     * @return The formatted message list
     * @throws std::invalid_argument If the message format is invalid or missing necessary fields
     * @throws std::runtime_error If the message type is not supported
     */
    json format_messages(const std::vector<Message>& messages);
    
    /**
     * @brief Send a request to the LLM and get the reply
     * @param messages The conversation message list
     * @param system_prompt Optional system message
     * @param next_step_prompt Optional prompt message for the next step
     * @param max_retries The maximum number of retries
     * @return The generated assistant content
     * @throws std::invalid_argument If the message is invalid or the reply is empty
     * @throws std::runtime_error If the API call fails
     */
    std::string ask(
        const std::vector<Message>& messages,
        const std::string& system_prompt = "",
        const std::string& next_step_prompt = "",
        int max_retries = 3
    );
    
    /**
     * @brief Send a request to the LLM with tool functions
     * @param messages The conversation message list
     * @param system_prompt Optional system message
     * @param next_step_prompt Optinonal prompt message for the next step
     * @param tools The tool list
     * @param tool_choice The tool choice strategy
     * @param max_retries The maximum number of retries
     * @return The generated assistant message (content, tool_calls)
     * @throws std::invalid_argument If the tool, tool choice or message is invalid
     * @throws std::runtime_error If the API call fails
     */
    json ask_tool(
        const std::vector<Message>& messages,
        const std::string& system_prompt = "",
        const std::string& next_step_prompt = "",
        const json& tools = {},
        const std::string& tool_choice = "auto",
        int max_retries = 3
    );

    size_t get_prompt_tokens() const {
        return total_prompt_tokens_;
    }

    size_t get_completion_tokens() const {
        return total_completion_tokens_;
    }

    void reset_tokens() {
        total_prompt_tokens_ = 0;
        total_completion_tokens_ = 0;
    }
};

} // namespace humanus

#endif // HUMANUS_LLM_H
