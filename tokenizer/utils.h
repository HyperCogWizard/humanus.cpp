#ifndef HUMANUS_TOKENIZER_UTILS_H
#define HUMANUS_TOKENIZER_UTILS_H

#include "base.h"
#include "../mcp/common/json.hpp"

namespace humanus {

using json = nlohmann::ordered_json;

/**
 * @brief Roughly count the number of tokens in a message (https://github.com/openai/openai-cookbook/blob/main/examples/How_to_count_tokens_with_tiktoken.ipynb)
 * @param tokenizer The tokenizer to use
 * @param messages The messages to count (object or array)
 * @return The number of tokens in the messages
 */
int num_tokens_from_messages(const BaseTokenizer& tokenizer, const json& messages);

/**
 * @brief Roughly count the number of tokens in a message (https://github.com/openai/openai-cookbook/blob/main/examples/How_to_count_tokens_with_tiktoken.ipynb)
 * @param tokenizer The tokenizer to use
 * @param tools The tools to count (array)
 * @param messages The messages to count (object or array)
 * @return The number of tokens in the messages
 */
int num_tokens_for_tools(const BaseTokenizer& tokenizer, const json& tools, const json& messages);

} // namespace humanus

#endif // HUMANUS_TOKENIZER_UTILS_H
