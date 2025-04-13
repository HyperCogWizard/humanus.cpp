#include "utils.h"

namespace humanus {

const std::filesystem::path PROJECT_ROOT = get_project_root();

size_t validate_utf8(const std::string& text) {
    size_t len = text.size();
    if (len == 0) return 0;

    // Check the last few bytes to see if a multi-byte character is cut off
    for (size_t i = 1; i <= 4 && i <= len; ++i) {
        unsigned char c = text[len - i];
        // Check for start of a multi-byte sequence from the end
        if ((c & 0xE0) == 0xC0) {
            // 2-byte character start: 110xxxxx
            // Needs at least 2 bytes
            if (i < 2) return len - i;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte character start: 1110xxxx
            // Needs at least 3 bytes
            if (i < 3) return len - i;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte character start: 11110xxx
            // Needs at least 4 bytes
            if (i < 4) return len - i;
        }
    }

    // If no cut-off multi-byte character is found, return full length
    return len;
}

bool readline_utf8(std::string & line, bool multiline_input) {
#if defined(_WIN32)
    std::wstring wline;
    if (!std::getline(std::wcin, wline)) {
        // Input stream is bad or EOF received
        line.clear();
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        return false;
    }

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wline[0], (int)wline.size(), NULL, 0, NULL, NULL);
    line.resize(size_needed);
    WideCharToMultiByte(CP_UTF8, 0, &wline[0], (int)wline.size(), &line[0], size_needed, NULL, NULL);
#else
    if (!std::getline(std::cin, line)) {
        // Input stream is bad or EOF received
        line.clear();
        return false;
    }
#endif
    if (!line.empty()) {
        char last = line.back();
        if (last == '/') { // Always return control on '/' symbol
            line.pop_back();
            return false;
        }
        if (last == '\\') { // '\\' changes the default action
            line.pop_back();
            multiline_input = !multiline_input;
        }
    }

    // By default, continue input if multiline_input is set
    return multiline_input;
}

// Parse the content of a message to a string
std::string parse_json_content(const json& content) {
    if (content.is_string()) {
        return content.get<std::string>();
    } else if (content.is_array()) {
        std::string result;
        int image_cnt = 0;
        for (const auto& item : content) {
            if (item["type"] == "text") {
                result += item["text"].get<std::string>();
            } else if (item["type"] == "image_url") {
                result += "[image" + std::to_string(++image_cnt) + "]";
            }
        }
        return result;
    } else {
        return content.dump(2);
    }
}

} // namespace humanus