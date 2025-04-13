#include "agent/humanus.h"
#include "logger.h"
#include "prompt.h"
#include "flow/flow_factory.h"

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <signal.h>
#include <unistd.h>
#elif defined (_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <signal.h>
#endif

using namespace humanus;

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__)) || defined (_WIN32)
static void sigint_handler(int signo) {
    if (signo == SIGINT) {
        // make sure all logs are flushed
        logger->info("Interrupted by user\n");
        logger->flush();
        _exit(130);
    }
}
#endif

int main() {

    // ctrl+C handling
    {
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
        struct sigaction sigint_action;
        sigint_action.sa_handler = sigint_handler;
        sigemptyset (&sigint_action.sa_mask);
        sigint_action.sa_flags = 0;
        sigaction(SIGINT, &sigint_action, NULL);
#elif defined (_WIN32)
        auto console_ctrl_handler = +[](DWORD ctrl_type) -> BOOL {
            return (ctrl_type == CTRL_C_EVENT) ? (sigint_handler(SIGINT), true) : false;
        };
        SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(console_ctrl_handler), true);
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        _setmode(_fileno(stdin), _O_WTEXT); // wide character input mode
#endif
    }

    const auto& config_data = toml::parse_file((PROJECT_ROOT / "config" / "config.toml").string());

    if (!config_data.contains("humanus_plan")) {
        throw std::runtime_error("humanus_plan section not found in config.toml");
    }

    const auto& config_table = *config_data["humanus_plan"].as_table();

    auto agent = std::make_shared<Humanus>(Humanus::load_from_toml(config_table));

    std::map<std::string, std::shared_ptr<BaseAgent>> agents;
    agents["default"] = agent;

    auto flow = FlowFactory::create_flow(
        FlowType::PLANNING,
        agent->llm,
        agents,
        "default" // primary_agent_key
    );

    while (true) {
        if (agent->current_step == agent->max_steps) {
            std::cout << "Automatically paused after " << agent->current_step << " steps." << std::endl;
            std::cout << "Enter your prompt (enter an empty line to resume or 'exit' to quit): ";
            agent->reset(false);
        } else if (agent->state != AgentState::IDLE) {
            std::cout << "Enter your prompt (enter an empty line to retry or 'exit' to quit): ";
            agent->reset(false);
        } else {
            std::cout << "Enter your prompt (or 'exit' to quit): ";
        }
        
        std::string prompt;
        readline_utf8(prompt, false);
        if (prompt == "exit") {
            logger->info("Goodbye!");
            break;
        }

        logger->info("Processing your request: " + prompt);
        auto result = flow->execute(prompt);
        logger->info("🌟 " + agent->name + "'s summary: " + result);
    }
}