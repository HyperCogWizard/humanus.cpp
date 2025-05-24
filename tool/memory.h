#ifndef HUMANUS_TOOL_MEMORY_H
#define HUMANUS_TOOL_MEMORY_H

#include "base.h"

namespace humanus {

struct MemoryTool : BaseTool {
    inline static const std::string name_ = "memory";
    inline static const std::string description_ = "Manage and retrieve memory.";
    inline static const json parameters_ = json::parse(R"json({
        "type": "object",
        "properties": {
            "events": {
                "description": "Array of memory events. Each event is an object with 'id', 'text', 'type', and 'old_memory' (optional) fields.",
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "id": {
                            "description": "Unique identifier for the memory item.",
                            "type": "string"
                        },
                        "text": {
                            "description": "Text of the memory item.",
                            "type": "string"
                        },
                        "type": {
                            "description": "Type of event: 'ADD', 'UPDATE', 'DELETE', or 'NONE'.",
                            "type": "string",
                            "enum": ["ADD", "UPDATE", "DELETE", "NONE"]
                        },
                        "old_memory": {
                            "description": "Old memory item. Required for update events.",
                            "type": "string"
                        }
                    }
                }
            }
        },
        "required": ["events"]
    })json");

    MemoryTool() : BaseTool(name_, description_, parameters_) {}
    
    ToolResult execute(const json& arguments) override {
        return ToolResult(arguments["events"]);
    }
};

} // namespace humanus


#endif // HUMANUS_TOOL_MEMORY_H