#include "../tokenizer/bpe.h"
#include "../tokenizer/utils.h"
#include "../mcp/common/json.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <stdexcept>

using namespace humanus;
using json = nlohmann::ordered_json;

#define TEST_FAILED(func, ...) std::cout << func << " \033[31mfailed\033[0m " << ("\0", ##__VA_ARGS__) << std::endl;
#define TEST_PASSED(func, ...) std::cout << func << " \033[32mpassed\033[0m " << ("\0", ##__VA_ARGS__) << std::endl;

void test_encode_decode(const BPETokenizer& tokenizer) {
    std::string test_text = "Hello, world! 你好，世界！";
    
    auto tokens = tokenizer.encode(test_text);
    
    std::string decoded = tokenizer.decode(tokens);
    
    if (decoded != test_text) {
        TEST_FAILED(__func__, "Expected " + test_text + ", got " + decoded);
        return;
    } 

    test_text = "お誕生日おめでとう";
    tokens = tokenizer.encode(test_text);
    decoded = tokenizer.decode(tokens);
    if (decoded != test_text) {
        TEST_FAILED(__func__, "Expected " + test_text + ", got " + decoded);
        return;
    } 

    std::vector<size_t> expected_tokens{33334, 45918, 243, 21990, 9080, 33334, 62004, 16556, 78699};
    if (tokens.size() != expected_tokens.size()) {
        TEST_FAILED(__func__, "Expected " + std::to_string(expected_tokens.size()) + " tokens, got " + std::to_string(tokens.size()));
        return;
    }

    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] != expected_tokens[i]) {
            TEST_FAILED(__func__, "Expected " + std::to_string(expected_tokens[i]) + ", got " + std::to_string(tokens[i]));
            return;
        }
    }

    TEST_PASSED(__func__);
}

void test_num_tokens_from_messages(const BPETokenizer& tokenizer) {
    json example_messages = json::parse(R"json([
        {
            "role": "system",
            "content": "You are a helpful, pattern-following assistant that translates corporate jargon into plain English."
        },
        {
            "role": "system",
            "name": "example_user",
            "content": "New synergies will help drive top-line growth."
        },
        {
            "role": "system",
            "name": "example_assistant",
            "content": "Things working well together will increase revenue."
        },
        {
            "role": "system",
            "name": "example_user",
            "content": "Let's circle back when we have more bandwidth to touch base on opportunities for increased leverage."
        },
        {
            "role": "system",
            "name": "example_assistant",
            "content": "Let's talk later when we're less busy about how to do better."
        },
        {
            "role": "user",
            "content": "This late pivot means we don't have time to boil the ocean for the client deliverable."
        }
    ])json");

    size_t num_tokens = num_tokens_from_messages(tokenizer, example_messages);

    if (num_tokens != 129) {
        TEST_FAILED(__func__, "Expected 129, got " + std::to_string(num_tokens));
    } else {
        TEST_PASSED(__func__);
    }
}

void test_num_tokens_for_tools(const BPETokenizer& tokenizer) {
    json tools = json::parse(R"json([
        {
            "type": "function",
            "function": {
                "name": "get_current_weather",
                "description": "Get the current weather in a given location",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "location": {
                            "type": "string",
                            "description": "The city and state, e.g. San Francisco, CA"
                        },
                        "unit": {
                            "type": "string",
                            "description": "The unit of temperature to return",
                            "enum": [
                                "celsius",
                                "fahrenheit"
                            ]
                        }
                    },
                    "required": [
                        "location"
                    ]
                }
            }
        }
    ])json");

    json example_messages = json::parse(R"json([
        {
            "role": "system",
            "content": "You are a helpful assistant that can answer to questions about the weather."
        },
        {
            "role": "user",
            "content": "What's the weather like in San Francisco?"
        }
    ])json");

    size_t num_tokens = num_tokens_for_tools(tokenizer, tools, example_messages);

    if (num_tokens != 105) {
        TEST_FAILED(__func__, "Expected 105, got " + std::to_string(num_tokens));
    } else {
        TEST_PASSED(__func__);
    }
}

int main() {
    try {
        auto tokenizer = BPETokenizer::load_from_tiktoken("tokenizer/cl100k_base.tiktoken");

        test_encode_decode(*tokenizer);

        test_num_tokens_from_messages(*tokenizer);

        test_num_tokens_for_tools(*tokenizer);

        return 0;
    } catch (const std::exception& e) {
        TEST_FAILED("test_bpe", "Error: " + std::string(e.what()));
        return 1;
    }
} 