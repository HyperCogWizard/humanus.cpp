#ifndef HUMANUS_AGENT_CHATBOT_H
#define HUMANUS_AGENT_CHATBOT_H

#include "base.h"

namespace humanus {

struct Chatbot : BaseAgent {

    Chatbot(
        const std::string& name = "chatbot",
        const std::string& description = "A chatbot agent",
        const std::string& system_prompt = "You are a helpful assistant.",
        const std::shared_ptr<LLM>& llm = nullptr,
        const std::shared_ptr<BaseMemory>& memory = nullptr
    ) : BaseAgent(
        name,
        description,
        system_prompt,
        "",
        llm,
        memory
    ) {}
    
    std::string run(const std::string& request = "") override {
        if (!request.empty()) {
            update_memory("user", request);
        }

        // Get response with tool options
        auto response = llm->ask(
            memory->get_messages(request),
            system_prompt
        );

        // Update memory with response
        update_memory("assistant", response);

        return response;
    }
};


}

#endif // HUMANUS_AGENT_CHATBOT_H