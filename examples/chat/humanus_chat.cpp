#include "agent/chatbot.h"
#include "logger.h"
#include "prompt.h"
#include "flow/flow_factory.h"
#include "memory/base.h"

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
    
    Chatbot chatbot{
        "chatbot", // name
        "A chatbot agent that uses memory to remember conversation history", // description
        "You are a helpful assistant.", // system_prompt
        LLM::get_instance("chatbot"), // llm
        std::make_shared<Memory>(Config::get_memory_config("chatbot")) // memory
    };

    while (true) {
        std::cout << "> ";

        std::string prompt;
        readline_utf8(prompt, false);

        if (prompt == "exit") {
            logger->info("Goodbye!");
            break;
        }

        logger->info("Processing your request: " + prompt);
        auto response = chatbot.run(prompt);
        logger->info("✨ " + chatbot.name + "'s response: " + response);
    }

    return 0;
}