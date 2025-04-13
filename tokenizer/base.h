#ifndef HUMANUS_TOKENIZER_BASE_H
#define HUMANUS_TOKENIZER_BASE_H

#include <vector>
#include <string>

namespace humanus {

class BaseTokenizer {
public:
    virtual std::vector<size_t> encode(const std::string& text) const = 0;
    virtual std::string decode(const std::vector<size_t>& tokens) const = 0;
};

}

#endif // HUMANUS_TOKENIZER_BASE_H