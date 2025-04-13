#ifndef HUMANUS_TOOL_PYTHON_EXECUTE_H
#define HUMANUS_TOOL_PYTHON_EXECUTE_H

#include "base.h"
#include "mcp_client.h"

namespace humanus {

struct PythonExecute : BaseMCPTool {
    inline static const std::string name_ = "python_execute";
    inline static const std::string description_ = "Executes Python code string. Note: Only print outputs are visible, function return values are not captured. Use print statements to see results.";
    inline static const json parameters_ = {
        {"type", "object"},
        {"properties", {
            {"code", {
                {"type", "string"},
                {"description", "The Python code to execute. Note: Use absolute file paths if code will read/write files."}
            }}
        }},
        {"required", {"code"}}
    };

    PythonExecute() : BaseMCPTool(name_, description_, parameters_) {}
};

}

#endif // HUMANUS_TOOL_PYTHON_EXECUTE_H