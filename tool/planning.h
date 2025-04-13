#ifndef HUMANUS_TOOL_PLANNING_H
#define HUMANUS_TOOL_PLANNING_H

#include "base.h"
#include "prompt.h"

namespace humanus {

struct PlanningTool : BaseTool {
    inline static const std::string name_ = "planning";
    inline static const std::string description_ = "Plan and track your tasks.";
    inline static const json parameters_ = json::parse(R"json({
        "type": "object",
        "properties": {
            "command": {
                "description": "The command to execute. Available commands: create, update, list, get, set_active, mark_step, delete.",
                "enum": [
                    "create",
                    "update",
                    "list",
                    "get",
                    "set_active",
                    "mark_step",
                    "delete"
                ],
                "type": "string"
            },
            "plan_id": {
                "description": "Unique identifier for the plan. Required for create, update, set_active, and delete commands. Optional for get and mark_step (uses active plan if not specified).",
                "type": "string"
            },
            "title": {
                "description": "Title for the plan. Required for create command, optional for update command.",
                "type": "string"
            },
            "steps": {
                "description": "List of plan steps. Required for create command, optional for update command.",
                "type": "array",
                "items": {"type": "string"}
            },
            "step_index": {
                "description": "Index of the step to update (0-based). Required for mark_step command.",
                "type": "integer"
            },
            "step_status": {
                "description": "Status to set for a step. Used with mark_step command.",
                "enum": ["not_started", "in_progress", "completed", "blocked"],
                "type": "string"
            },
            "step_notes": {
                "description": "Additional notes for a step. Optional for mark_step command.",
                "type": "string"
            }
        },
        "required": ["command"],
        "additionalProperties": false
    })json");

    
    std::map<std::string, json> plans; // Dictionary to store plans by plan_id
    std::string _current_plan_id; // Track the current active plan

    PlanningTool() : BaseTool(name_, description_, parameters_) {}

    /**
     * Execute the planning tool with the given command and parameters.
     *
     * Parameters:
     * - command: The operation to perform
     * - plan_id: Unique identifier for the plan
     * - title: Title for the plan (used with create command)
     * - steps: List of steps for the plan (used with create command)
     * - step_index: Index of the step to update (used with mark_step command)
     * - step_status: Status to set for a step (used with mark_step command)
     * - step_notes: Additional notes for a step (used with mark_step command)
     */
    ToolResult execute(const json& args) override;

    // Create a new plan with the given ID, title, and steps.
    ToolResult _create_plan(const std::string& plan_id, const std::string& title, const std::vector<std::string>& steps);

    // Update an existing plan with new title or steps.
    ToolResult _update_plan(const std::string& plan_id, const std::string& title, const std::vector<std::string>& steps);

    // List all available plans.
    ToolResult _list_plans();

    // Get details of a specific plan.
    ToolResult _get_plan(const std::string& plan_id);

    // Set a plan as the active plan.
    ToolResult _set_active_plan(const std::string& plan_id);

    // Mark a step with a specific status and optional notes.
    ToolResult _mark_step(const std::string& plan_id, int step_index, const std::string& step_status, const std::string& step_notes);

    // Delete a plan.
    ToolResult _delete_plan(const std::string& plan_id);

    // Format a plan for display.
    std::string _format_plan(const json& plan);
};

} // namespace humanus

#endif // HUMANUS_TOOL_PLANNING_H
