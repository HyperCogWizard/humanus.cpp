#include "httplib.h"
#include "agent/humanus.h"
#include "logger.h"
#include "mcp_server.h"
#include "mcp_tool.h"
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <random>
#include <unordered_map>

using namespace humanus;

static auto session_sink = SessionSink::get_instance();

class SessionManager {
public:
    std::shared_ptr<Humanus> get_agent(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = agents_.find(session_id);
        if (it != agents_.end()) {
            return it->second;
        }
        
        auto agent = std::make_shared<Humanus>();
        agents_[session_id] = agent;

        return agent;
    }

    void set_agent(const std::string& session_id, const std::shared_ptr<Humanus>& agent) {
        std::lock_guard<std::mutex> lock(mutex_);
        agents_[session_id] = agent;
    }

    static std::vector<std::string> get_logs_buffer(const std::string& session_id) {
        return session_sink->get_buffer(session_id);
    }

    static std::vector<std::string> get_logs_history(const std::string& session_id) {
        return session_sink->get_history(session_id);
    }

    void set_result(const std::string& session_id, const std::string& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        results_[session_id] = result;
    }

    std::string get_result(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = results_.find(session_id);
        if (it != results_.end()) {
            return it->second;
        }

        return "";
    }

    void clear_result(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        results_.erase(session_id);
    }
    
    bool has_session(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return agents_.find(session_id) != agents_.end();
    }
    
    void close_session(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        agents_.erase(session_id);
        results_.erase(session_id);
        session_sink->cleanup_session(session_id);
    }
    
    std::vector<std::string> get_all_sessions() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> sessions;
        for (const auto& pair : agents_) {
            sessions.push_back(pair.first);
        }
        return sessions;
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Humanus>> agents_;
    std::unordered_map<std::string, std::string> results_;
    std::unordered_map<std::string, std::shared_ptr<SessionSink>> session_sinks_;
};

int main(int argc, char** argv) {
#if defined (_WIN32)
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdin), _O_WTEXT); // wide character input mode
#endif

    mcp::set_log_level(mcp::log_level::warning);

    int port = 8896;

    if (argc == 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port number: " << argv[1] << std::endl;
            return 1;
        }
    }

    // Create and configure server
    mcp::server server("localhost", port, "humanus_server", "0.1.0");
    
    // Set server capabilities
    mcp::json capabilities = {
        {"tools", mcp::json::object()}
    };
    server.set_capabilities(capabilities);

    auto session_manager = std::make_shared<SessionManager>();

    auto initialize_tool = mcp::tool_builder("humanus_initialize")
                    .with_description("Initialize the agent")
                    .with_string_param("llm", "The LLM configuration to use. Default: default")
                    .with_string_param("memory", "The memory configuration to use. Default: default")
                    .with_array_param("tools", "The tools of the agent. Default: filesystem, playwright (for browser use), image_loader, content_provider, terminate", "string")
                    .with_array_param("mcp_servers", "The MCP servers offering tools for the agent. Default: python_execute", "string")
                    .with_number_param("max_steps", "The maximum steps of the agent. Default: 30")
                    .with_number_param("duplicate_threshold", "The duplicate threshold of the agent. Default: 2")
                    .build();

    server.register_tool(initialize_tool, [session_manager](const json& args, const std::string& session_id) -> json {
        if (session_manager->has_session(session_id)) {
            throw mcp::mcp_exception(mcp::error_code::invalid_request, "Session already initialized");
        }

        try {
            session_manager->set_agent(session_id, std::make_shared<Humanus>(Humanus::load_from_json(args)));
        } catch (const std::exception& e) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Invalid agent configuration: " + std::string(e.what()));
        }
        
        return {{
            {"type", "text"},
            {"text", "Agent initialized."}
        }};
    });
    
    auto run_tool = mcp::tool_builder("humanus_run")
                    .with_description("Request to start a new task. Best to give clear and concise prompts.")
                    .with_string_param("prompt", "The prompt text to process", true)
                    .build();
    
    server.register_tool(run_tool, [session_manager](const json& args, const std::string& session_id) -> json {
        if (!args.contains("prompt")) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing `prompt` parameter");
        }

        std::string prompt = args["prompt"].get<std::string>();
        
        auto agent = session_manager->get_agent(session_id);
        
        if (agent->state != AgentState::IDLE) {
            throw mcp::mcp_exception(mcp::error_code::invalid_request, "The agent is busy, please wait for the current task to complete or terminate the current task.");
        }

        agent->reset();

        session_manager->clear_result(session_id);

        std::thread([agent, session_manager, prompt, session_id]() {           
            try {
                session_sink->set_session_id(session_id); 
                logger->info("Processing your request: " + prompt);
                auto result = agent->run(prompt);
                logger->info("Task completed.");
                session_manager->set_result(session_id, result);
            } catch (const std::exception& e) {
                logger->error("Session {} error: {}", session_id, e.what());
            }
        }).detach();
        
        return {{
            {"type", "text"},
            {"text", "Task started, call `humanus_status` to check the status."}
        }};
    });

    auto terminate_tool = mcp::tool_builder("humanus_terminate")
                          .with_description("Terminate the current task")
                          .build();
    
    server.register_tool(terminate_tool, [session_manager](const json& args, const std::string& session_id) -> json {
        if (!session_manager->has_session(session_id)) {
            throw mcp::mcp_exception(mcp::error_code::invalid_request, "Session not found");
        }
        
        auto agent = session_manager->get_agent(session_id);
        
        if (agent->state == AgentState::IDLE) {
            return {{
                {"type", "text"},
                {"text", "The agent is idle, no task to terminate."}
            }};
        }
        
        agent->update_memory("user", "User interrupted the interaction. Consider rescheduling the previous task or switching to a different task according to the user's request.");
        agent->state = AgentState::IDLE;
        
        logger->info("Task terminated by user.");

        return {{
            {"type", "text"},
            {"text", "Task terminated."}
        }};
    });    
    
    auto status_tool = mcp::tool_builder("humanus_status")
                    .with_description("Get the status of the current task.")
                    .build();
    
    server.register_tool(status_tool, [session_manager](const json& args, const std::string& session_id) -> json {
        if (!session_manager->has_session(session_id)) {
            throw mcp::mcp_exception(mcp::error_code::invalid_request, "Session not found");
        }
        
        auto agent = session_manager->get_agent(session_id);
        auto result = session_manager->get_result(session_id);

        json status = {
            {"state", agent_state_map[agent->state]},
            {"current_step", agent->current_step},
            {"max_steps", agent->max_steps},
            {"prompt_tokens", agent->get_prompt_tokens()},
            {"completion_tokens", agent->get_completion_tokens()},
            {"log_buffer", session_sink->get_buffer(session_id)},
            {"result", result}
        };
        
        return {{
            {"type", "text"},
            {"text", status.dump(2)}
        }};
    });

    // Start server
    std::cout << "Starting Humanus server at http://localhost:" << port << "..." << std::endl;
    std::cout << "Press Ctrl+C to stop server" << std::endl;
    server.start(true);  // Blocking mode
    
    return 0;
}

