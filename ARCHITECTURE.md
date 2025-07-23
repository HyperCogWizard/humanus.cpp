# Technical Architecture Documentation

## Table of Contents

1. [System Overview](#system-overview)
2. [Core Architecture](#core-architecture)
3. [Component Details](#component-details)
4. [Data Flow](#data-flow)
5. [Module Interactions](#module-interactions)
6. [Deployment Architecture](#deployment-architecture)
7. [Development Guidelines](#development-guidelines)

## System Overview

**humanus.cpp** is a lightweight C++ framework for building local LLM agents with integrated tool support and memory capabilities. The system is designed around modularity, performance, and extensibility.

```mermaid
graph TB
    subgraph "humanus.cpp Framework"
        subgraph "Core Layer"
            LLM[LLM Interface]
            Config[Configuration]
            Logger[Logging System]
            Schema[Data Schemas]
        end
        
        subgraph "Agent Layer"
            BaseAgent[Base Agent]
            ToolCallAgent[ToolCall Agent]
            Humanus[Humanus Agent]
            PlanningAgent[Planning Agent]
        end
        
        subgraph "Tool Layer"
            ToolCollection[Tool Collection]
            PythonExec[Python Execute]
            Filesystem[File System]
            WebTools[Web Tools]
            Memory[Memory Tools]
        end
        
        subgraph "Memory Layer"
            BaseMemory[Base Memory]
            VectorStore[Vector Store]
            EmbeddingModel[Embedding Model]
        end
        
        subgraph "Integration Layer"
            MCP[Model Context Protocol]
            Server[MCP Server]
            Client[Client Interfaces]
        end
    end
    
    subgraph "External Systems"
        LLMProviders[LLM Providers<br/>OpenAI, Local Models]
        MCPServers[External MCP Servers]
        FileSystem[File System]
        WebBrowser[Web Browsers]
    end
    
    LLM --> LLMProviders
    MCP --> MCPServers
    ToolCollection --> FileSystem
    WebTools --> WebBrowser
    
    BaseAgent --> LLM
    BaseAgent --> BaseMemory
    ToolCallAgent --> ToolCollection
    Humanus --> ToolCallAgent
    PlanningAgent --> BaseAgent
    
    VectorStore --> EmbeddingModel
    BaseMemory --> VectorStore
```

### Key Design Principles

- **Modularity**: Clear separation between agents, tools, memory, and integration layers
- **Performance**: C++ implementation for minimal overhead and fast execution
- **Extensibility**: Plugin-style architecture for tools and memory backends
- **Interoperability**: MCP integration for standardized tool communication

## Core Architecture

### Agent Hierarchy

The agent system follows an inheritance-based design with increasing specialization:

```mermaid
classDiagram
    class BaseAgent {
        +string name
        +string description
        +string system_prompt
        +string next_step_prompt
        +shared_ptr~LLM~ llm
        +shared_ptr~BaseMemory~ memory
        +AgentState state
        +int max_steps
        +int current_step
        +run(request) string
        +step() virtual string
        +reset(clear_memory) void
        +update_memory(role, content) void
    }
    
    class ToolCallAgent {
        +ToolCollection available_tools
        +string tool_choice
        +vector~string~ special_tool_names
        +execute_tool_calls(tool_calls) ToolResult
        +step() string override
    }
    
    class PlanningAgent {
        +vector~shared_ptr~BaseAgent~~ executors
        +shared_ptr~BaseAgent~ current_executor
        +step() string override
        +plan_and_execute() string
    }
    
    class Humanus {
        +Humanus()
        // Specialized configuration for general-purpose tasks
    }
    
    BaseAgent <|-- ToolCallAgent
    BaseAgent <|-- PlanningAgent
    ToolCallAgent <|-- Humanus
```

### Tool System Architecture

The tool system provides a flexible plugin architecture:

```mermaid
classDiagram
    class BaseTool {
        <<interface>>
        +get_name() string
        +get_description() string
        +get_parameters() ToolParameters
        +execute(arguments) ToolResult
    }
    
    class ToolCollection {
        +vector~shared_ptr~BaseTool~~ tools
        +map~string, shared_ptr~BaseTool~~ tool_map
        +add_tool(tool) void
        +get_tool(name) shared_ptr~BaseTool~
        +execute_tool(name, args) ToolResult
        +get_tool_schemas() vector~ToolSchema~
    }
    
    class PythonExecute {
        +execute(arguments) ToolResult
        +run_python_code(code) string
    }
    
    class Filesystem {
        +execute(arguments) ToolResult
        +read_file(path) string
        +write_file(path, content) void
        +list_directory(path) vector~string~
    }
    
    class Playwright {
        +execute(arguments) ToolResult
        +navigate(url) void
        +click(selector) void
        +extract_content() string
    }
    
    class Terminate {
        +execute(arguments) ToolResult
        +reason string
    }
    
    BaseTool <|.. PythonExecute
    BaseTool <|.. Filesystem
    BaseTool <|.. Playwright
    BaseTool <|.. Terminate
    ToolCollection o-- BaseTool
```

### Memory System Architecture

The memory system provides both simple message storage and advanced vector-based retrieval:

```mermaid
classDiagram
    class BaseMemory {
        +deque~Message~ messages
        +string current_request
        +add_message(message) bool
        +add_messages(messages) bool
        +get_messages(query) vector~Message~
        +clear() void
    }
    
    class Memory {
        +shared_ptr~VectorStore~ vector_store
        +shared_ptr~EmbeddingModel~ embedding_model
        +MemoryConfig config
        +add_message(message, tool_calls) bool
        +get_messages(query) vector~Message~
        +search_memories(query, limit) vector~Message~
    }
    
    class VectorStore {
        <<interface>>
        +add(vectors, metadatas) void
        +search(query_vector, k) vector~pair~
        +delete_vectors(ids) void
    }
    
    class HNSWVectorStore {
        +unique_ptr~hnswlib::HierarchicalNSW~ index
        +add(vectors, metadatas) void
        +search(query_vector, k) vector~pair~
    }
    
    class EmbeddingModel {
        <<interface>>
        +embed_text(text) vector~float~
        +embed_texts(texts) vector~vector~float~~
    }
    
    class OpenAIEmbedding {
        +unique_ptr~httplib::Client~ client
        +EmbeddingConfig config
        +embed_text(text) vector~float~
    }
    
    BaseMemory <|-- Memory
    VectorStore <|.. HNSWVectorStore
    EmbeddingModel <|.. OpenAIEmbedding
    Memory o-- VectorStore
    Memory o-- EmbeddingModel
```

## Component Details

### LLM Interface Layer

The LLM interface provides a unified API for different language model providers:

```mermaid
sequenceDiagram
    participant Agent
    participant LLM
    participant HTTPClient
    participant Provider as LLM Provider

    Agent->>LLM: complete(messages, tools)
    LLM->>LLM: prepare_request(messages, tools)
    LLM->>HTTPClient: POST /v1/chat/completions
    HTTPClient->>Provider: API Request
    Provider-->>HTTPClient: Response
    HTTPClient-->>LLM: HTTP Response
    LLM->>LLM: parse_response()
    LLM-->>Agent: CompletionResponse
```

### Model Context Protocol (MCP) Integration

MCP enables standardized communication with external tools and services:

```mermaid
graph TB
    subgraph "humanus.cpp Process"
        Agent[Agent]
        MCPClient[MCP Client]
    end
    
    subgraph "External MCP Server"
        MCPServer[MCP Server]
        Tool[Tool Implementation]
    end
    
    subgraph "Communication"
        STDIO[STDIO]
        SSE[Server-Sent Events]
    end
    
    Agent --> MCPClient
    MCPClient --> STDIO
    MCPClient --> SSE
    STDIO --> MCPServer
    SSE --> MCPServer
    MCPServer --> Tool
    
    Tool --> MCPServer
    MCPServer --> STDIO
    MCPServer --> SSE
    STDIO --> MCPClient
    SSE --> MCPClient
    MCPClient --> Agent
```

### Configuration System

The configuration system manages settings across different components:

```mermaid
graph LR
    subgraph "Configuration Files"
        ConfigLLM[config_llm.toml]
        ConfigMemory[config_memory.toml]
        ConfigAgent[config_agent.toml]
        ConfigMCP[config_mcp.toml]
    end
    
    subgraph "Configuration Classes"
        LLMConfig[LLMConfig]
        MemoryConfig[MemoryConfig]
        AgentConfig[AgentConfig]
        MCPConfig[MCPConfig]
    end
    
    subgraph "Core Components"
        LLM[LLM Instance]
        Memory[Memory Instance]
        Agent[Agent Instance]
        MCP[MCP Client]
    end
    
    ConfigLLM --> LLMConfig
    ConfigMemory --> MemoryConfig
    ConfigAgent --> AgentConfig
    ConfigMCP --> MCPConfig
    
    LLMConfig --> LLM
    MemoryConfig --> Memory
    AgentConfig --> Agent
    MCPConfig --> MCP
```

## Data Flow

### Agent Execution Flow

```mermaid
sequenceDiagram
    participant User
    participant Agent
    participant LLM
    participant Tools
    participant Memory

    User->>Agent: run(request)
    Agent->>Memory: add_message(user, request)
    
    loop until task complete or max_steps
        Agent->>Memory: get_messages()
        Memory-->>Agent: conversation_history
        Agent->>LLM: complete(messages, tools)
        LLM-->>Agent: response_with_tool_calls
        
        alt has tool calls
            Agent->>Tools: execute_tool_calls()
            Tools-->>Agent: tool_results
            Agent->>Memory: add_message(assistant, response)
            Agent->>Memory: add_message(tool, results)
        else no tool calls
            Agent->>Memory: add_message(assistant, response)
            Agent-->>User: final_response
        end
    end
```

### Memory Retrieval Flow

```mermaid
sequenceDiagram
    participant Agent
    participant Memory
    participant EmbeddingModel
    participant VectorStore

    Agent->>Memory: get_messages(query)
    Memory->>EmbeddingModel: embed_text(query)
    EmbeddingModel-->>Memory: query_vector
    Memory->>VectorStore: search(query_vector, k=5)
    VectorStore-->>Memory: relevant_message_ids
    Memory->>Memory: retrieve_messages(ids)
    Memory-->>Agent: relevant_messages + recent_messages
```

### Tool Execution Flow

```mermaid
sequenceDiagram
    participant Agent
    participant ToolCollection
    participant Tool
    participant ExternalSystem

    Agent->>ToolCollection: execute_tool(name, arguments)
    ToolCollection->>ToolCollection: validate_arguments()
    ToolCollection->>Tool: execute(arguments)
    
    alt MCP Tool
        Tool->>ExternalSystem: MCP request
        ExternalSystem-->>Tool: MCP response
    else Native Tool
        Tool->>Tool: execute_logic()
    end
    
    Tool-->>ToolCollection: ToolResult
    ToolCollection-->>Agent: ToolResult
```

## Module Interactions

### Core Dependencies

```mermaid
graph TB
    subgraph "Application Layer"
        CLI[CLI Applications]
        Server[Server Applications]
    end
    
    subgraph "Agent Layer"
        Agents[Agent Implementations]
    end
    
    subgraph "Service Layer"
        LLM[LLM Service]
        Memory[Memory Service]
        Tools[Tool Service]
        MCP[MCP Service]
    end
    
    subgraph "Infrastructure Layer"
        Config[Configuration]
        Logger[Logging]
        Schema[Schema Definitions]
        Utils[Utilities]
    end
    
    subgraph "External Dependencies"
        OpenSSL[OpenSSL]
        HNSWLib[HNSWLib]
        TomlPlusPlus[toml++]
        SPDLog[spdlog]
    end
    
    CLI --> Agents
    Server --> Agents
    Agents --> LLM
    Agents --> Memory
    Agents --> Tools
    Agents --> MCP
    
    LLM --> Config
    Memory --> Config
    Tools --> Config
    MCP --> Config
    
    LLM --> Logger
    Memory --> Logger
    Tools --> Logger
    MCP --> Logger
    
    Config --> TomlPlusPlus
    Logger --> SPDLog
    LLM --> OpenSSL
    Memory --> HNSWLib
```

### Build System Architecture

```mermaid
graph TB
    subgraph "CMake Build System"
        RootCMake[CMakeLists.txt]
        MCPModule[mcp/CMakeLists.txt]
        ServerModule[server/CMakeLists.txt]
        TokenizerModule[tokenizer/CMakeLists.txt]
        TestsModule[tests/CMakeLists.txt]
        ExamplesModule[examples/CMakeLists.txt]
    end
    
    subgraph "Build Targets"
        HumanusLib[libhumanus.a]
        MCPLib[libmcp.a]
        ServerLib[libserver.a]
        Executables[Various Executables]
    end
    
    subgraph "Source Organization"
        SrcFiles[src/*.cpp]
        AgentFiles[agent/*.cpp]
        ToolFiles[tool/*.cpp]
        FlowFiles[flow/*.cpp]
        MemoryFiles[memory/*.cpp]
        TokenizerFiles[tokenizer/*.cpp]
    end
    
    RootCMake --> MCPModule
    RootCMake --> ServerModule
    RootCMake --> TokenizerModule
    RootCMake --> TestsModule
    RootCMake --> ExamplesModule
    
    SrcFiles --> HumanusLib
    AgentFiles --> HumanusLib
    ToolFiles --> HumanusLib
    FlowFiles --> HumanusLib
    MemoryFiles --> HumanusLib
    TokenizerFiles --> HumanusLib
    
    MCPModule --> MCPLib
    ServerModule --> ServerLib
    
    HumanusLib --> Executables
    MCPLib --> Executables
    ServerLib --> Executables
```

## Deployment Architecture

### Local Development Setup

```mermaid
graph TB
    subgraph "Development Environment"
        SourceCode[Source Code]
        BuildDir[build/]
        ConfigDir[config/]
    end
    
    subgraph "Build Artifacts"
        Libraries[Libraries<br/>libhumanus.a<br/>libmcp.a]
        Executables[Executables<br/>humanus_cli<br/>humanus_server<br/>mcp_server]
    end
    
    subgraph "Configuration"
        LLMConfig[config_llm.toml]
        MemoryConfig[config_memory.toml]
        MCPConfig[config_mcp.toml]
    end
    
    subgraph "External Services"
        LLMProvider[LLM Provider<br/>OpenAI/Local]
        MCPServers[External MCP Servers]
    end
    
    SourceCode --> Libraries
    Libraries --> Executables
    ConfigDir --> LLMConfig
    ConfigDir --> MemoryConfig
    ConfigDir --> MCPConfig
    
    Executables --> LLMProvider
    Executables --> MCPServers
```

### Production Deployment Options

```mermaid
graph TB
    subgraph "Deployment Option 1: Standalone"
        Standalone[humanus_cli]
        LocalLLM[Local LLM Server<br/>llama.cpp]
        LocalMCP[Local MCP Servers]
    end
    
    subgraph "Deployment Option 2: Server Mode"
        HumanusServer[humanus_server]
        LoadBalancer[Load Balancer]
        MultipleClients[Multiple Clients]
    end
    
    subgraph "Deployment Option 3: Embedded"
        Application[Host Application]
        HumanusLib[libhumanus.a]
        CustomTools[Custom Tools]
    end
    
    Standalone --> LocalLLM
    Standalone --> LocalMCP
    
    MultipleClients --> LoadBalancer
    LoadBalancer --> HumanusServer
    
    Application --> HumanusLib
    HumanusLib --> CustomTools
```

## Development Guidelines

### Adding New Tools

1. **Inherit from BaseTool**: Implement the required interface methods
2. **Define Tool Schema**: Specify parameters and descriptions
3. **Register with ToolCollection**: Add to available tools
4. **Handle MCP Integration**: If needed, implement MCP client communication

```cpp
class CustomTool : public BaseTool {
public:
    std::string get_name() override { return "custom_tool"; }
    std::string get_description() override { return "Description"; }
    ToolParameters get_parameters() override { /* define params */ }
    ToolResult execute(const json& arguments) override { /* implementation */ }
};
```

### Adding New Agent Types

1. **Inherit from BaseAgent or ToolCallAgent**: Choose appropriate base class
2. **Override step() method**: Implement agent-specific logic
3. **Configure prompts**: Define system and next-step prompts
4. **Set up tools**: Configure available tool collection

### Memory Backend Integration

1. **Implement VectorStore interface**: For new vector database backends
2. **Implement EmbeddingModel interface**: For new embedding providers
3. **Update MemoryConfig**: Add configuration options
4. **Test integration**: Ensure compatibility with existing memory system

### Best Practices

- **Error Handling**: Use exceptions for unrecoverable errors, return error states for recoverable ones
- **Logging**: Use the integrated spdlog system for consistent logging
- **Configuration**: Use TOML files for all configuration needs
- **Memory Management**: Use smart pointers for automatic memory management
- **Thread Safety**: Consider thread safety for server deployments

This architecture documentation provides a comprehensive overview of the humanus.cpp framework's design, components, and usage patterns. The modular design enables easy extension and customization while maintaining performance and reliability.