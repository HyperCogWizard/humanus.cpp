#ifndef HUMANUS_TOOL_FILESYSTEM_H
#define HUMANUS_TOOL_FILESYSTEM_H

#include "base.h"

namespace humanus {

// https://github.com/modelcontextprotocol/servers/tree/HEAD/src/filesystem
struct Filesystem : BaseMCPTool {
    inline static const std::string name_ = "filesystem";
    inline static const std::string description_ = "## Features\n\n- Read/write files\n- Create/list/delete directories\n- Move files/directories\n- Search files\n- Get file metadata";
    inline static const json parameters_ = json::parse(R"json({
        "type": "object",
        "properties": {
            "command": {
                "type": "string",
                "description": "### Commands\n\n- **read_file**\n  - Read complete contents of a file\n  - Input: `path` (string)\n  - Reads complete file contents with UTF-8 encoding\n\n- **read_multiple_files**\n  - Read multiple files simultaneously\n  - Input: `paths` (string[])\n  - Failed reads won't stop the entire operation\n\n- **write_file**\n  - Create new file or overwrite existing (exercise caution with this)\n  - Inputs:\n    - `path` (string): File location\n    - `content` (string): File content\n\n- **edit_file**\n  - Make selective edits using advanced pattern matching and formatting\n  - Features:\n    - Line-based and multi-line content matching\n    - Whitespace normalization with indentation preservation\n    - Fuzzy matching with confidence scoring\n    - Multiple simultaneous edits with correct positioning\n    - Indentation style detection and preservation\n    - Git-style diff output with context\n    - Preview changes with dry run mode\n    - Failed match debugging with confidence scores\n  - Inputs:\n    - `path` (string): File to edit\n    - `edits` (array): List of edit operations\n      - `oldText` (string): Text to search for (can be substring)\n      - `newText` (string): Text to replace with\n    - `dryRun` (boolean): Preview changes without applying (default: false)\n    - `options` (object): Optional formatting settings\n      - `preserveIndentation` (boolean): Keep existing indentation (default: true)\n      - `normalizeWhitespace` (boolean): Normalize spaces while preserving structure (default: true)\n      - `partialMatch` (boolean): Enable fuzzy matching (default: true)\n  - Returns detailed diff and match information for dry runs, otherwise applies changes\n  - Best Practice: Always use dryRun first to preview changes before applying them\n\n- **create_directory**\n  - Create new directory or ensure it exists\n  - Input: `path` (string)\n  - Creates parent directories if needed\n  - Succeeds silently if directory exists\n\n- **list_directory**\n  - List directory contents with [FILE] or [DIR] prefixes\n  - Input: `path` (string)\n\n- **move_file**\n  - Move or rename files and directories\n  - Inputs:\n    - `source` (string)\n    - `destination` (string)\n  - Fails if destination exists\n\n- **search_files**\n  - Recursively search for files/directories\n  - Inputs:\n    - `path` (string): Starting directory\n    - `pattern` (string): Search pattern\n    - `excludePatterns` (string[]): Exclude any patterns. Glob formats are supported.\n  - Case-insensitive matching\n  - Returns full paths to matches\n\n- **get_file_info**\n  - Get detailed file/directory metadata\n  - Input: `path` (string)\n  - Returns:\n    - Size\n    - Creation time\n    - Modified time\n    - Access time\n    - Type (file/directory)\n    - Permissions\n\n- **list_allowed_directories**\n  - List all directories the server is allowed to access\n  - No input required\n  - Returns:\n    - Directories that this server can read/write from",
                "enum": [
                    "read_file",
                    "read_multiple_files", 
                    "write_file",
                    "edit_file",
                    "create_directory",
                    "list_directory",
                    "move_file",
                    "search_files",
                    "get_file_info",
                    "list_allowed_directories"
                ]
            },
            "path": {
                "type": "string",
                "description": "The path to the file or directory to operate on. Only works within allowed directories. Required by all commands except `read_multiple_files`, `move_file` and `list_allowed_directories`."
            },
            "paths": {
                "type": "array",
                "description": "An array of paths to files to operate on. Only works within allowed directories. Required by `read_multiple_files`.",
                "items": {
                    "type": "string"
                }
            },
            "content": {
                "type": "string",
                "description": "The content to write to the file. Required by `write_file`."
            },
            "edits": {
                "type": "array",
                "description": "Each edit replaces exact line sequences with new content. Required by `edit_file`."
            },
            "dryRun": {
                "type": "boolean",
                "description": "Preview changes without applying. Default: false. Required by `edit_file`."
            },
            "options": {
                "type": "object",
                "description": "Optional formatting settings. Required by `edit_file`.",
                "properties": {
                    "preserveIndentation": {
                        "type": "boolean",
                        "description": "Keep existing indentation. Default: true. Required by `edit_file`."
                    },
                    "normalizeWhitespace": {
                        "type": "boolean",
                        "description": "Normalize spaces while preserving structure. Default: true. Required by `edit_file`."
                    },
                    "partialMatch": {
                        "type": "boolean",
                        "description": "Enable fuzzy matching. Default: true. Required by `edit_file`."
                    }
                }
            },
            "source": {
                "type": "string",
                "description": "The source path to move or rename. Required by `move_file`."
            },
            "destination": {
                "type": "string",
                "description": "The destination path to move or rename. Required by `move_file`."
            },
            "pattern": {
                "type": "string",
                "description": "The pattern to search for. Required by `search_files`."
            },
            "excludePatterns": {
                "type": "array",
                "description": "An array of patterns to exclude from the search. Glob formats are supported. Required by `search_files`.",
                "items": {
                    "type": "string"
                }
            }
        },
        "required": ["command"]
    })json");

    inline static std::set<std::string> allowed_commands = {
        "read_file",
        "read_multiple_files", 
        "write_file",
        "edit_file",
        "create_directory",
        "list_directory",
        "move_file",
        "search_files",
        "get_file_info",
        "list_allowed_directories"
    };

    Filesystem() : BaseMCPTool(name_, description_, parameters_) {}

    ToolResult execute(const json& args) override {
        try {
            if (!_client) {
                return ToolError("Failed to initialize shell client");
            }
            
            std::string command;
            if (args.contains("command")) {
                if (args["command"].is_string()) {
                    command = args["command"].get<std::string>();
                } else {
                    return ToolError("Invalid command format");
                }
            } else {
                return ToolError("'command' is required");
            }
            
            if (allowed_commands.find(command) == allowed_commands.end()) {
                return ToolError("Unknown command '" + command + "'. Please use one of the following commands: " + 
                                 std::accumulate(allowed_commands.begin(), allowed_commands.end(), std::string(),
                                                 [](const std::string& a, const std::string& b) {
                                                     return a + (a.empty() ? "" : ", ") + b;
                                                 }));
            }

            json result = _client->call_tool(command, args);

            bool is_error = result.value("isError", false);
            
            // Return different ToolResult based on whether there is an error
            if (is_error) {
                return ToolError(result.value("content", json::array()));
            } else {
                return ToolResult(result.value("content", json::array()));
            }
        } catch (const std::exception& e) {
            return ToolError(std::string(e.what()));
        }
    }
};

}

#endif // HUMANUS_TOOL_FILE_SAVER_H
