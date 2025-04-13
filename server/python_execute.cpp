/**
 * @file python_execute.cpp
 * 
 * This file implements the Python execution tool, using Python.h to directly call the Python interpreter.
 */

#include "mcp_server.h"
#include "mcp_tool.h"
#include "mcp_resource.h"

#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <chrono>
#include <future>

// Check if Python is found
#ifdef PYTHON_FOUND
#include <Python.h>
#endif

/**
 * @class python_interpreter
 * @brief Python interpreter class for executing Python code with session support
 */
class python_interpreter {
private:
    mutable std::shared_mutex py_mutex;
    bool is_initialized;
    
    // Map to store Python thread states for each session
    mutable std::unordered_map<std::string, PyThreadState*> session_states;
    
    // Default timeout (milliseconds)
    static constexpr unsigned int DEFAULT_TIMEOUT_MS = 30000; // 30 seconds

public:
    /**
     * @brief Constructor, initializes Python interpreter
     */
    python_interpreter() : is_initialized(false) {
#ifdef PYTHON_FOUND
        try {
            std::unique_lock<std::shared_mutex> lock(py_mutex);
            Py_Initialize();
            if (Py_IsInitialized()) {
                is_initialized = true;
                // Create main thread state
                PyThreadState* main_thread_state = PyThreadState_Get();
                PyThreadState_Swap(NULL);
            } else {
                std::cerr << "Failed to initialize Python interpreter" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Python interpreter initialization exception: " << e.what() << std::endl;
        }
#endif
    }

    /**
     * @brief Destructor, releases Python interpreter
     */
    ~python_interpreter() {
#ifdef PYTHON_FOUND
        if (is_initialized) {
            std::unique_lock<std::shared_mutex> lock(py_mutex);
            // Clean up all session states
            for (auto& pair : session_states) {
                PyThreadState_Swap(pair.second);
                PyThreadState_Clear(pair.second);
                PyThreadState_Delete(pair.second);
            }
            session_states.clear();
            
            Py_Finalize();
            is_initialized = false;
        }
#endif
    }
    
    /**
     * @brief Check if a session exists
     * @param session_id The session identifier
     * @return bool indicating if session exists
     */
#ifdef PYTHON_FOUND
    bool has_session(const std::string& session_id) const {
        std::shared_lock<std::shared_mutex> lock(py_mutex);
        return session_states.find(session_id) != session_states.end();
    }

    /**
     * @brief Get an existing thread state for a session
     * @param session_id The session identifier
     * @return PyThreadState for the session or nullptr if not found
     */
    PyThreadState* get_existing_session_state(const std::string& session_id) const {
        std::shared_lock<std::shared_mutex> lock(py_mutex);
        auto it = session_states.find(session_id);
        if (it != session_states.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    /**
     * @brief Create a new thread state for a session
     * @param session_id The session identifier
     * @return PyThreadState for the new session
     */
    PyThreadState* create_session_state(const std::string& session_id) const {
        std::unique_lock<std::shared_mutex> lock(py_mutex);
        if (session_states.count(session_id)) return session_states[session_id];

        PyThreadState* new_state = Py_NewInterpreter();
        if (!new_state) {
            throw std::runtime_error("Failed to create new Python interpreter for session " + session_id);
        }
        PyEval_SaveThread();  // Release GIL after creation
        session_states[session_id] = new_state;
        return new_state;
    }

    
    /**
     * @brief Get or create a thread state for a session
     * @param session_id The session identifier
     * @return PyThreadState for the session
     */
    PyThreadState* get_session_state(const std::string& session_id) const {
        // Try to get existing session state with read lock
        PyThreadState* state = get_existing_session_state(session_id);
        if (state) {
            return state;
        }
        
        // If it doesn't exist, create a new session (will use write lock)
        return create_session_state(session_id);
    }
#endif

    /**
     * @brief Execute Python code in the context of a specific session with timeout
     * @param input JSON object containing Python code
     * @param session_id The session identifier
     * @return JSON object with execution results
     */
    mcp::json forward(const mcp::json& input, const std::string& session_id) {
#ifdef PYTHON_FOUND
    if (!is_initialized) {
        return mcp::json{{"error", "Python interpreter not properly initialized"}};
    }

    unsigned int timeout_ms = DEFAULT_TIMEOUT_MS;
    if (input.contains("timeout_ms") && input["timeout_ms"].is_number()) {
        timeout_ms = input["timeout_ms"].get<unsigned int>();
    }

    std::packaged_task<mcp::json()> task([this, &input, session_id]() {
        mcp::json thread_result;

        PyThreadState* tstate = nullptr;
        try {
            tstate = get_session_state(session_id);
        } catch (const std::exception& e) {
            return mcp::json{{"error", e.what()}};
        }

        PyEval_RestoreThread(tstate);

        try {
            if (input.contains("code") && input["code"].is_string()) {
                std::string code = input["code"].get<std::string>();

                PyObject *main_module = PyImport_AddModule("__main__");
                PyObject *main_dict = PyModule_GetDict(main_module);

                PyObject *io_module = PyImport_ImportModule("io");
                PyObject *string_io = PyObject_GetAttrString(io_module, "StringIO");
                PyObject *sys_stdout = PyObject_CallObject(string_io, nullptr);
                PyObject *sys_stderr = PyObject_CallObject(string_io, nullptr);

                PySys_SetObject("stdout", sys_stdout);
                PySys_SetObject("stderr", sys_stderr);

                PyObject *result = PyRun_String(code.c_str(), Py_file_input, main_dict, main_dict);
                if (!result) PyErr_Print();
                Py_XDECREF(result);

                PyObject *out_value = PyObject_CallMethod(sys_stdout, "getvalue", nullptr);
                PyObject *err_value = PyObject_CallMethod(sys_stderr, "getvalue", nullptr);

                std::string output = out_value && PyUnicode_Check(out_value) ? PyUnicode_AsUTF8(out_value) : "";
                std::string error = err_value && PyUnicode_Check(err_value) ? PyUnicode_AsUTF8(err_value) : "";

                Py_XDECREF(out_value);
                Py_XDECREF(err_value);
                Py_DECREF(sys_stdout);
                Py_DECREF(sys_stderr);
                Py_DECREF(string_io);
                Py_DECREF(io_module);

                if (!output.empty()) thread_result["output"] = output;
                if (!error.empty()) thread_result["error"] = error;

                if (thread_result.empty()) {
                    thread_result["warning"] = "No output generated. Consider using print statements.";
                }
            } else {
                thread_result["error"] = "Invalid parameters or code not provided";
            }
        } catch (const std::exception& e) {
            thread_result["error"] = std::string("Python execution exception: ") + e.what();
        }

        tstate = PyEval_SaveThread();  // Save thread state and release GIL
        return thread_result;
    });

    auto future = task.get_future();
    std::thread execution_thread(std::move(task));

    if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
        if (execution_thread.joinable()) execution_thread.detach();  // detach to prevent zombie threads
        return mcp::json{{"error", "Python execution timed out after " + std::to_string(timeout_ms) + "ms"}};
    }

    if (execution_thread.joinable()) execution_thread.join();  // Join thread if it's still running

    return future.get();
#else
    return mcp::json{{"error", "Python interpreter not available"}};
#endif
    }

    
    /**
     * @brief Clean up a session and remove its thread state
     * @param session_id The session identifier to clean up
     */
    void cleanup_session(const std::string& session_id) {
#ifdef PYTHON_FOUND
        std::unique_lock<std::shared_mutex> lock(py_mutex);
        auto it = session_states.find(session_id);
        if (it != session_states.end()) {
            PyThreadState* state = it->second;
            PyEval_RestoreThread(state);
            Py_EndInterpreter(state);  // Properly end the child interpreter
            session_states.erase(it);
            PyEval_SaveThread();  // Restore the main thread's GIL release state
        }
#endif
    }

};

// Global Python interpreter instance
static python_interpreter interpreter;

// Python execution tool handler function
mcp::json python_execute_handler(const mcp::json& args, const std::string& session_id) {
    if (!args.contains("code")) {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'code' parameter");
    }
    
    try {
        // Use Python interpreter to execute code with session context and timeout
        mcp::json result = interpreter.forward(args, session_id);
        
        return {{
            {"type", "text"},
            {"text", result.dump(2)}
        }};
    } catch (const std::exception& e) {
        throw mcp::mcp_exception(mcp::error_code::internal_error, 
                                "Failed to execute Python code: " + std::string(e.what()));
    }
}

// Register the PythonExecute tool
void register_python_execute_tool(mcp::server& server) {
    mcp::tool python_tool = mcp::tool_builder("python_execute")
        .with_description("Executes Python code string. Note: Only print outputs are visible, function return values are not captured. Use print statements to see results.")
        .with_string_param("code", "The Python code to execute. Note: Use absolute file paths if code will read/write files.", true)
        .with_number_param("timeout_ms", "Timeout in milliseconds for code execution (default: 30000)", false)
        .build();
    
    server.register_tool(python_tool, python_execute_handler);
    
    // Register session cleanup handler
    server.register_session_cleanup("python_execute", [](const std::string& session_id) {
        interpreter.cleanup_session(session_id);
    });
}