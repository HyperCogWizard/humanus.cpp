/**
 * @file mcp_server_main.cpp
 * @brief Humanus MCP Server Implementation
 * 
 * This file implements the Humanus MCP server that provides tool invocation functionality.
 * Currently implements the PythonExecute tool.
 */

#include "mcp_server.h"
#include "mcp_tool.h"
#include "mcp_resource.h"

#include <iostream>
#include <string>
#include <memory>
#include <filesystem>

// Import Python execution tool
extern void register_python_execute_tool(mcp::server& server);

int main(int argc, char* argv[]) {
    int port = 8895;

    if (argc == 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port number: " << argv[1] << std::endl;
            return 1;
        }
    }

    // Create and configure server
    mcp::server server("localhost", port);
    server.set_server_info("humanus_tool", "0.1.0");
    
    // Set server capabilities
    mcp::json capabilities = {
        {"tools", mcp::json::object()}
    };
    server.set_capabilities(capabilities);
    
    // Register Python execution tool
    register_python_execute_tool(server);
    
    // Start server
    std::cout << "Starting Humanus MCP server at localhost:" << port << "..." << std::endl;
    std::cout << "Press Ctrl+C to stop server" << std::endl;
    server.start(true);  // Blocking mode
    
    return 0;
}