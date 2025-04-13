#ifndef HUMANUS_AGENT_REACT_H
#define HUMANUS_AGENT_REACT_H

#include "base.h"

namespace humanus {

struct ReActAgent : BaseAgent {
    ReActAgent(
        const std::string& name, 
        const std::string& description, 
        const std::string& system_prompt,
        const std::string& next_step_prompt,
        const std::shared_ptr<LLM>& llm = nullptr,
        const std::shared_ptr<BaseMemory>& memory = nullptr,
        int max_steps = 10,
        int duplicate_threshold = 2
    ) : BaseAgent(
            name, 
            description, 
            system_prompt, 
            next_step_prompt, 
            llm, 
            memory, 
            max_steps,
            duplicate_threshold
        ) {}

    // Process current state and decide next actions using tools
    virtual bool think() = 0;

    // Execute decided actions
    virtual std::string act() = 0;

    // Execute a single step: think and act.
    virtual std::string step() {
        bool should_act = think();
        if (!should_act) {
            return "Thinking complete - no action needed";
        }
        if (state == AgentState::RUNNING) {
            return act();
        }
        return "Agent is not running";
    }
};

}

#endif // HUMANUS_AGENT_REACT_H
