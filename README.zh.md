<p align="center">
  <img src="assets/humanus.png" width="200"/>
</p>

[English](README.md) | 中文

[![GitHub stars](https://img.shields.io/github/stars/WHU-MYTH-Lab/humanus.cpp?style=social)](https://github.com/WHU-MYTH-Lab/humanus.cpp/stargazers) &ensp;
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) &ensp;


# humanus.cpp

**Humanus**（拉丁语意为"人类"）是一个基于 [OpenManus](https://github.com/mannaandpoem/OpenManus) 和 [mem0](https://github.com/mem0ai/mem0) 启发的**轻量级 C++ 框架**，集成了模型上下文协议（MCP, Model Context Protocol）。本项目旨在为构建本地 LLM 智能体提供快速、模块化的基础。

**主要特点：**
- **C++ 实现**：核心逻辑为 C++，优化速度并最小化开销
- **轻量级设计**：最少的依赖和简单的架构，非常适合嵌入式或资源受限的环境
- **跨平台兼容**：支持 Linux、macOS 和 Windows
- **MCP 协议集成**：通过 MCP 原生支持标准化工具交互
- **向量化记忆**：使用基于 HNSW 的相似度搜索进行上下文检索
- **模块化架构**：易于插入新的模型、工具或存储后端

**Humanus 仍处于早期阶段** — 这是一个正在进行中的工作，正在快速发展。我们在开放地迭代，不断改进，并始终欢迎反馈、想法和贡献。

让我们一起探索使用 **humanus.cpp** 构建本地 LLM 智能体的潜力！

## 项目演示

<video src="https://private-user-images.githubusercontent.com/54173798/433116754-6e0b8c07-7ead-4e25-8fec-de3a3031f583.mp4?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3NDQ1MzI0NzMsIm5iZiI6MTc0NDUzMjE3MywicGF0aCI6Ii81NDE3Mzc5OC80MzMxMTY3NTQtNmUwYjhjMDctN2VhZC00ZTI1LThmZWMtZGUzYTMwMzFmNTgzLm1wND9YLUFtei1BbGdvcml0aG09QVdTNC1ITUFDLVNIQTI1NiZYLUFtei1DcmVkZW50aWFsPUFLSUFWQ09EWUxTQTUzUFFLNFpBJTJGMjAyNTA0MTMlMkZ1cy1lYXN0LTElMkZzMyUyRmF3czRfcmVxdWVzdCZYLUFtei1EYXRlPTIwMjUwNDEzVDA4MTYxM1omWC1BbXotRXhwaXJlcz0zMDAmWC1BbXotU2lnbmF0dXJlPWQzZDM4MGMzZjExN2RhNDE5ZDFhYWMzYmZkMjJiZjI0ZDE1MTk1Mzk0YjFkNzhjYjhlZjBhOWI5NTRhZDJmNjMmWC1BbXotU2lnbmVkSGVhZGVycz1ob3N0In0.PMbjc8jfhyTQHrCisJzNNjdllLART95rPDY5E1A2vM8" 
       controls 
       muted 
       style="max-height:640px; border:1px solid #ccc; border-radius:8px;">
</video>

## 如何构建

```bash
git submodule update --init

cmake -B build
cmake --build build --config Release
```

## 如何运行

### 配置

要设置自定义配置，请按照以下步骤操作：

1. 将 `config/example` 中的所有文件复制到 `config`。
2. 根据需要，在 `config/config_llm.toml` 中替换 `base_url`、`api_key` 等，以及 `config/config*.toml` 中的其他配置。
    > Note：[llama.cpp](https://github.com/ggml-org/llama.cpp) 中的 `llama-server` 也支持用于向量化记忆的嵌入模型。
3. 在 `"@modelcontextprotocol/server-filesystem"` 后填写 `args` 以控制对文件的访问。例如：
```
[filesystem]
type = "stdio"
command = "npx"
args = ["-y",
        "@modelcontextprotocol/server-filesystem",
        "/Users/{Username}/Desktop",
        "other/path/to/your/files] # Allowed paths
```

### `mcp_server`

（目前工具仅以 `python_execute` 为例）

在端口 8895 上启动带有 `python_execute` 工具的 MCP 服务器（或将端口作为参数传递）：
```bash
./build/bin/mcp_server <port> # Unix/MacOS
```

```shell
.\build\bin\Release\mcp_server.exe <port> # Windows
```

### `humanus_cli`

运行带有 `python_execute`、`filesystem` 和 `playwright`（用于浏览器）工具的默认智能体：

```bash
./build/bin/humanus_cli # Unix/MacOS
```

```shell
.\build\bin\Release\humanus_cli.exe # Windows
```

### `humanus_cli_plan`（开发中）

运行规划流程（仅使用 `humanus` 智能体作为执行器）：
```bash
./build/bin/humanus_cli_plan # Unix/MacOS
```

```shell
.\build\bin\Release\humanus_cli_plan.exe # Windows
```

### `humanus_server`（开发中）

在 MCP 服务器中运行智能体（默认运行在端口 8896）：
- `humanus_initialze`：传递 JSON 配置（如 `config/config.toml` 中）以初始化会话的智能体。（每个会话/客户端只维护一个智能体）
- `humanus_run`：传递 `prompt` 告诉智能体要做什么。（一次只能执行一个任务）
- `humanus_terminate`：停止当前任务。
- `humanus_status`：获取智能体和任务的当前状态及其他信息。返回：
  - `state`：智能体状态。
  - `current_step`：智能体的当前步骤索引。
  - `max_steps`：无需与用户交互的最大执行步骤数。
  - `prompt_tokens`：提示（输入）token 消耗。
  - `completion_tokens`：完成（输出）token 消耗。
  - `log_buffer`：缓冲区中的日志，类似 `humanus_cli`。获取后将被清除。
  - `result`：解释智能体的工作过程，任务未完成时为空。

```bash
./build/bin/humanus_server <port> # Unix/MacOS
```

```shell
.\build\bin\Release\humanus_cli_plan.exe <port> # Windows
```

在 Cursor 中配置：
```json
{
  "mcpServers": {
    "humanus": {
      "url": "http://localhost:8896/sse"
    }
  }
}
```

> 实验性功能：MCP 中的 MCP！可以运行 `humanus_server` 并从另一个 MCP 服务器或 `humanus_cli` 与它互动。

## 致谢

<p align="center">
  <img src="assets/whu.png" height="180"/>
  <img src="assets/myth.png" height="180"/>
</p>

本工作得到了中国国家自然科学基金（编号：62306216）与湖北省自然科学基金（编号：2023AFB816）的资助。

## 引用

```bibtex
@misc{humanus_cpp,
  author = {Zihong Zhang and Zuchao Li},
  title = {humanus.cpp: A Lightweight C++ Framework for Local LLM Agents},
  year = {2025}
}
```