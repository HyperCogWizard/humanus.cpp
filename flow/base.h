#ifndef HUMANUS_FLOW_BASE_H
#define HUMANUS_FLOW_BASE_H

#include "tool/base.h"
#include "agent/base.h"

namespace humanus {

enum FlowType {
    PLANNING = 0
};

const std::map<FlowType, std::string> FLOW_TYPE_MAP = {
    {PLANNING, "planning"}
};

// Base class for execution flows supporting multiple agents
struct BaseFlow {
    std::map<std::string, std::shared_ptr<BaseAgent>> agents;
    std::string primary_agent_key;

    BaseFlow(const std::map<std::string, std::shared_ptr<BaseAgent>>& agents = {}, const std::string& primary_agent_key = "") 
    : agents(agents), primary_agent_key(primary_agent_key) {
        // If primary agent not specified, use first agent
        if (primary_agent_key.empty() && !agents.empty()) {
            this->primary_agent_key = agents.begin()->first;
        }
    }

    BaseFlow(const std::shared_ptr<BaseAgent>& agent, const std::string& primary_agent_key = "") 
    : primary_agent_key(primary_agent_key) {
        agents["default"] = agent;
        // If primary agent not specified, use first agent
        if (primary_agent_key.empty()) {
            this->primary_agent_key = "default";
        }
    }

    BaseFlow(const std::vector<std::shared_ptr<BaseAgent>>& agents_list, const std::string& primary_agent_key = "")
    : primary_agent_key(primary_agent_key) {
        for (size_t i = 0; i < agents_list.size(); i++) {
            agents["agent_" + std::to_string(i)] = agents_list[i];
        }
        // If primary agent not specified, use first agent
        if (primary_agent_key.empty() && !agents.empty()) {
            this->primary_agent_key = agents.begin()->first;
        }
    }

    virtual ~BaseFlow() = default;

    // Get the primary agent for the flow
    std::shared_ptr<BaseAgent> primary_agent() const {
        return agents.at(primary_agent_key);
    }

    // Get a specific agent by key
    std::shared_ptr<BaseAgent> get_agent(const std::string& key) const {
        return agents.at(key);
    }

    // Add a new agent to the flow
    void add_agent(const std::string& key, const std::shared_ptr<BaseAgent>& agent) {
        agents[key] = agent;
    }

    // Execute the flow with the given input
    virtual std::string execute(const std::string& input_text) = 0;
};

}

#endif // HUMANUS_FLOW_BASE_H
