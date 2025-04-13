#ifndef HUMANUS_FLOW_PLANNING_H
#define HUMANUS_FLOW_PLANNING_H

#include "base.h"
#include "agent/base.h"
#include "agent/toolcall.h"
#include "llm.h"
#include "logger.h"
#include "schema.h"
#include "tool/planning.h"
#include <regex>

namespace humanus {

// A flow that manages planning and execution of tasks using agents.
struct PlanningFlow : public BaseFlow {
    std::shared_ptr<LLM> llm;
    std::shared_ptr<PlanningTool> planning_tool;
    std::string active_plan_id;
    int current_step_index;

    PlanningFlow(
        const std::shared_ptr<LLM>& llm = nullptr,
        const std::map<std::string, std::shared_ptr<BaseAgent>>& agents = {},
        const std::string& primary_agent_key = "default"
    ) : BaseFlow(agents, primary_agent_key),
        llm(llm) {
        if (!llm) {
            this->llm = primary_agent()->llm;
        }
        planning_tool = std::make_shared<PlanningTool>();
        reset();
    }
    
    // Get an appropriate executor agent for the current step.
    // Can be extended to select agents based on step type/requirements.
    std::shared_ptr<BaseAgent> get_executor(const std::string& step_type = "") const;

    // Execute the planning flow with agents.
    std::string execute(const std::string& input) override;

    // Create an initial plan based on the request using the flow's LLM and PlanningTool.
    void _create_initial_plan(const std::string& request);

    // Parse the current plan to identify the first non-completed step's index and info.
    // Returns (None, None) if no active step is found.
    void _get_current_step_info(int& current_step_index, json& step_info);

    // Execute the current step with the specified agent using agent.run().
    std::string _execute_step(const std::shared_ptr<BaseAgent>& executor, const json& step_info);

    // Mark the current step as completed.
    void _mark_step_completed();

    // Get the current plan as formatted text.
    std::string _get_plan_text();
    
    // Generate plan text directly from storage if the planning tool fails.
    std::string _generate_plan_text_from_storage();

    // Summarize the plan using the flow's LLM directly
    std::string _summarize_plan(const std::vector<Message> messages);

    // Reset the flow to its initial state.
    void reset(bool reset_memory = true) {
        active_plan_id = "plan_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        current_step_index = -1;
        for (const auto& [key, agent] : agents) {
            agent->reset(reset_memory);
        }
    }
};

}

#endif // HUMANUS_FLOW_PLANNING_H
