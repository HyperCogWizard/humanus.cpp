#ifndef HUMANUS_TOOL_FACT_EXTRACT_H
#define HUMANUS_TOOL_FACT_EXTRACT_H

#include "tool/base.h"

namespace humanus {

struct FactExtract : BaseTool {
    inline static const std::string name_ = "fact_extract";
    inline static const std::string description_ = "Extract facts and store them in a long-term memory.";
    inline static const json parameters_ = json::parse(R"json({
        "type": "object",
        "properties": {
            "facts": {
                "description": "List of facts to extract and store.",
                "type": "array",
                "items": {"type": "string"}
            }
        },
        "required": ["facts"],
        "additionalProperties": false
    })json");

    FactExtract() : BaseTool(name_, description_, parameters_) {}

    ToolResult execute(const json& arguments) override {
        return ToolResult(arguments["facts"]);
    }
};

}

#endif // HUMANUS_TOOL_FACT_EXTRACT_H