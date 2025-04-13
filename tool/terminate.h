#ifndef HUMANUS_TOOL_TERMINATE_H
#define HUMANUS_TOOL_TERMINATE_H

#include "base.h"
#include "prompt.h"

namespace humanus {

struct Terminate : BaseTool {
    inline static const std::string name_ = "terminate";
    inline static const std::string description_ = "Terminate the interaction when the request is met OR if the assistant cannot proceed further with the task.";
    inline static const humanus::json parameters_ = {
        {"type", "object"},
        {"properties", {
            {"status", {
                {"type", "string"},
                {"description", "The finish status of the interaction."},
                {"enum", {"success", "failure"}}
            }}
        }},
        {"required", {"status"}}
    };

    Terminate() : BaseTool(name_, description_, parameters_) {}

    // Finish the current execution
    ToolResult execute(const json& arguments) override {
        return ToolResult{
            "The interaction has been completed with status: " + arguments.value("status", "unknown")
        };
    }
};

}

#endif // HUMANUS_TOOL_TERMINATE_H