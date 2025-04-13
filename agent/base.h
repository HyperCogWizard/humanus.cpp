#ifndef HUMANUS_AGENT_BASE_H
#define HUMANUS_AGENT_BASE_H

#include "llm.h"
#include "schema.h"
#include "logger.h"
#include "memory/base.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace humanus {

/**
 * Abstract base class for managing agent state and execution.
 * Provides foundational functionality for state transitions, memory management,
 * and a step-based execution loop. Subclasses must implement the `step` method.
 */
struct BaseAgent : std::enable_shared_from_this<BaseAgent> {
    // Core attributes
    std::string name; // Unique name of the agent
    std::string description; // Optional agent description

    // Prompts
    std::string system_prompt; // System-level instruction prompt
    std::string next_step_prompt; // Prompt for determining next action

    // Dependencies
    std::shared_ptr<LLM> llm; // Language model instance
    std::shared_ptr<BaseMemory> memory; // Agent's memory store
    AgentState state; // Current state of the agent

    // Execution control
    int max_steps; // Maximum steps before termination
    int current_step; // Current step in execution

    int duplicate_threshold; // Threshold for duplicate messages

    BaseAgent(
        const std::string& name, 
        const std::string& description, 
        const std::string& system_prompt,
        const std::string& next_step_prompt,
        const std::shared_ptr<LLM>& llm = nullptr,
        const std::shared_ptr<BaseMemory>& memory = nullptr,
        int max_steps = 10,
        int duplicate_threshold = 2
    ) : name(name), 
        description(description), 
        system_prompt(system_prompt), 
        next_step_prompt(next_step_prompt), 
        llm(llm), 
        memory(memory), 
        max_steps(max_steps), 
        duplicate_threshold(duplicate_threshold) {
        initialize_agent();
    }

    // Initialize agent with default settings if not provided.
    void initialize_agent() {
        if (!llm) {
            llm = LLM::get_instance("default");
        }
        if (!memory) {
            memory = std::make_shared<Memory>(Config::get_memory_config("default"));
        }
        reset(true);
    }

    // Add a message to the agent's memory
    template<typename... Args>
    void update_memory(const std::string& role, const std::string& content, Args&&... args) {
        if (role == "user") {
            memory->add_message(Message::user_message(content));
        } else if (role == "assistant") {
            memory->add_message(Message::assistant_message(content), std::forward<Args>(args)...);
        } else if (role == "system") {
            memory->add_message(Message::system_message(content));
        } else if (role == "tool") {
            memory->add_message(Message::tool_message(content, std::forward<Args>(args)...));
        } else {
            throw std::runtime_error("Unsupported message role: " + role);
        }
    }

    // Execute the agent's main loop asynchronously
    virtual std::string run(const std::string& request = "") {
        memory->current_request = request;

        if (state != AgentState::IDLE) {
            logger->error("Cannot run agent from state " + agent_state_map[state]);
            return "Cannot run agent from state " + agent_state_map[state];
        }

        if (!request.empty()) {
            update_memory("user", request);
        }

        state = AgentState::RUNNING; // IDLE -> RUNNING
        std::vector<std::string> results;
        while (current_step < max_steps && state == AgentState::RUNNING) {
            current_step++;
            logger->info("Executing step " + std::to_string(current_step) + "/" + std::to_string(max_steps));
            std::string step_result;

            try {
                step_result = step();
            } catch (const std::exception& e) {
                logger->error("Error executing step " + std::to_string(current_step) + ": " + std::string(e.what()));
                state = AgentState::ERR;
                break;
            }

            if (is_stuck()) {
                handle_stuck_state();
            }

            results.push_back("Step " + std::to_string(current_step) + ": " + step_result);
        }

        if (current_step >= max_steps) {
            results.push_back("Terminated: Reached max steps (" + std::to_string(max_steps) + ")");
        }

        if (state != AgentState::FINISHED) {
            results.push_back("Terminated: Agent state is " + agent_state_map[state]);
        } else {
            state = AgentState::IDLE; // FINISHED -> IDLE
        }

        std::string result_str = "";

        for (const auto& result : results) {
            result_str += result + "\n";
        }

        if (result_str.empty()) {
            result_str = "No steps executed";
        }

        return result_str;
    }

    /**
     * Execute a single step in the agent's workflow.
     * Must be implemented by subclasses to define specific behavior.
     */
    virtual std::string step() {
        return "No step executed";
    }

    // Handle stuck state by adding a prompt to change strategy
    void handle_stuck_state() {
        std::string stuck_prompt = "Observed duplicate responses. Consider new strategies and avoid repeating ineffective paths already attempted.";
        logger->warn("Agent detected stuck state. Added prompt: " + stuck_prompt);
        memory->add_message(Message::user_message(stuck_prompt));
    }

    // O(n * m) LCS algorithm, could basically handle current LLM context
    size_t get_lcs_length(const std::string& s1, const std::string& s2) {
        std::vector<std::vector<size_t>> dp(s1.size() + 1, std::vector<size_t>(s2.size() + 1));
        for (size_t i = 1; i <= s1.size(); i++) {
            for (size_t j = 1; j <= s2.size(); j++) {
                if (s1[i - 1] == s2[j - 1]) {
                    dp[i][j] = dp[i - 1][j - 1] + 1;
                } else {
                    dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
                }
            }
        }
        return dp[s1.size()][s2.size()];
    }

    // Check if the agent is stuck in a loop by detecting duplicate content
    bool is_stuck() {
        const std::vector<Message>& messages = memory->get_messages();

        if (messages.size() < duplicate_threshold) {
            return false;
        }

        const Message& last_message = messages.back();
        if (last_message.content.empty() || last_message.role != "assistant") {
            return false;
        }

        // Count identical content occurrences
        int duplicate_count = 0;
        int duplicate_lcs_length = 0.6 * last_message.content.get<std::string>().size(); // TODO: make this threshold configurable
        for (auto r_it = messages.rbegin(); r_it != messages.rend(); ++r_it) {
            if (r_it == messages.rbegin()) {
                continue;
            }
            const Message& message = *r_it;
            if (message.role == "assistant" && !message.content.empty()) {
                if (get_lcs_length(message.content, last_message.content) > duplicate_lcs_length) {
                    duplicate_count++;
                    if (duplicate_count >= duplicate_threshold) {
                        break;
                    }
                }
            }
        }

        return duplicate_count >= duplicate_threshold;
    }

    void reset(bool reset_memory = true) {
        current_step = 0;
        state = AgentState::IDLE;
        llm->reset_tokens();
        if (reset_memory) {
            memory->clear();
        }
    }

    size_t get_prompt_tokens() {
        return llm->get_prompt_tokens();
    }

    size_t get_completion_tokens() {
        return llm->get_completion_tokens();
    }
};

} // namespace humanus

#endif // HUMANUS_AGENT_BASE_H
