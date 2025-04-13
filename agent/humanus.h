#ifndef HUMANUS_AGENT_HUMANUS_H
#define HUMANUS_AGENT_HUMANUS_H

#include "base.h"
#include "toolcall.h"
#include "prompt.h"
#include "tool/tool_collection.h"
#include "tool/python_execute.h"
#include "tool/puppeteer.h"
#include "tool/playwright.h"
#include "tool/filesystem.h"
#include "tool/image_loader.h"

namespace humanus {

/**
 * A versatile general-purpose agent that uses planning to solve various tasks.
 * 
 * This agent extends PlanningAgent with a comprehensive set of tools and capabilities,
 * including Python execution, web browsing, file operations, and information retrieval
 * to handle a wide range of user requests.
 */
struct Humanus : ToolCallAgent {
    Humanus(
        const ToolCollection& available_tools = ToolCollection( // Add general-purpose tools to the tool collection
            {
                std::make_shared<PythonExecute>(),
                std::make_shared<Filesystem>(),
                std::make_shared<Playwright>(),
                std::make_shared<ImageLoader>(),
                std::make_shared<ContentProvider>(),
                std::make_shared<Terminate>()
            }
        ),
        const std::string& name = "humanus",
        const std::string& description = "A versatile agent that can solve various tasks using multiple tools",
        const std::string& system_prompt = prompt::humanus::SYSTEM_PROMPT,
        const std::string& next_step_prompt = prompt::humanus::NEXT_STEP_PROMPT,
        const std::shared_ptr<LLM>& llm = nullptr,
        const std::shared_ptr<BaseMemory>& memory = nullptr,
        int max_steps = 30,
        int duplicate_threshold = 2
    ) : ToolCallAgent(
            available_tools,
            "auto", // tool_choice
            {"terminate"}, // special_tool_names
            name,
            description,
            system_prompt,
            next_step_prompt,
            llm,
            memory,
            max_steps,
            duplicate_threshold
        ) {}

    std::string run(const std::string& request = "") override;

    static Humanus load_from_toml(const toml::table& config_table);

    static Humanus load_from_json(const json& config_json);
};

}

#endif // HUMANUS_AGENT_HUMANUS_H
