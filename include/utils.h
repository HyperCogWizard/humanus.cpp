#ifndef HUMANUS_UTILS_H
#define HUMANUS_UTILS_H

#include "mcp_message.h"
#include <filesystem>
#include <iostream>

#if defined (_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace humanus {

using json = mcp::json;

// Get project root directory
inline std::filesystem::path get_project_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

extern const std::filesystem::path PROJECT_ROOT;

// return the last index of character that can form a valid string
// if the last character is potentially cut in half, return the index before the cut
// if validate_utf8(text) == text.size(), then the whole text is valid utf8
size_t validate_utf8(const std::string& text);

bool readline_utf8(std::string & line, bool multiline_input = false);

// Parse the content of a message to a string
std::string parse_json_content(const json& content);

} // namespace humanus

#endif
