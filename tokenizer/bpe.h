#ifndef HUMANUS_TOKENIZER_BPE_H
#define HUMANUS_TOKENIZER_BPE_H

#include "base.h"
#include "../mcp/common/base64.hpp"
#include <unordered_map>
#include <queue>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <memory>
#include <utility>
#include <functional>
#include <limits>

namespace humanus {

/**
 * @brief BPE (Byte Pair Encoding) Tokenizer Implementation
 * 
 * Uses tiktoken format vocabulary and merge rules file.   
 * Uses a priority queue for efficient BPE merging.
 */
class BPETokenizer : public BaseTokenizer {
private:
    // Helper structure for hashing pairs
    struct PairHash {
        template <class T1, class T2>
        std::size_t operator() (const std::pair<T1, T2>& pair) const {
            return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
        }
    };

    // UTF-8 bytes to token mapping
    std::unordered_map<std::string, size_t> encoder;
    // Token ID to UTF-8 bytes mapping
    std::unordered_map<size_t, std::string> decoder;
    
    // Merge priority mapping, lower rank with higher priority
    std::unordered_map<std::pair<std::string, std::string>, size_t, PairHash> merge_ranks;
    
    // Used by tiktoken merge ranks
    std::string base64_decode(const std::string& encoded) const {
        if (encoded.empty()) return "";
        return base64::decode(encoded);
    }
    
    // Lower rank with higher priority
    struct MergeComparator {
        const std::unordered_map<std::pair<std::string, std::string>, size_t, PairHash>& ranks;
        
        MergeComparator(const std::unordered_map<std::pair<std::string, std::string>, size_t, PairHash>& r) 
            : ranks(r) {}
        
        bool operator()(const std::pair<std::pair<std::string, std::string>, size_t>& a,
                       const std::pair<std::pair<std::string, std::string>, size_t>& b) const {
            // First compare by merge_ranks, if not exist then use max value
            size_t rank_a = ranks.count(a.first) ? ranks.at(a.first) : std::numeric_limits<size_t>::max();
            size_t rank_b = ranks.count(b.first) ? ranks.at(b.first) : std::numeric_limits<size_t>::max();
            
            // Max heap, so we need to reverse compare (smaller rank has higher priority)
            return rank_a > rank_b;
        }
    };

public:
    /**
     * @brief Construct BPE tokenizer from tiktoken format file
     * @param tokenizer_path path to tiktoken format vocabulary file
     * 
     * File format: Each line contains a base64 encoded token and its corresponding token ID
     * Example: "IQ== 0", where "IQ==" is the base64 encoded token and 0 is the corresponding ID
     */
    BPETokenizer(const std::string& tokenizer_path) {
        std::ifstream file(tokenizer_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open tokenizer file: " + tokenizer_path);
        }
        
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string token_base64;
            size_t rank;
            
            if (iss >> token_base64 >> rank) {
                std::string token = base64_decode(token_base64);
                
                encoder[token] = rank;
                decoder[rank] = token;
            }
        }
        
        build_merge_ranks();
    }
    
    /**
     * @brief Build merge priority mapping
     * 
     * Use tokens in vocabulary to infer possible merge rules.
     * For tokens longer than 1, try all possible splits, if the split parts are also in the vocabulary,
     * then assume this is a valid merge rule.
     */
    void build_merge_ranks() {
        for (const auto& [token, id] : encoder) {
            if (token.length() <= 1) continue;
            
            // Try all possible split points
            for (size_t i = 1; i < token.length(); ++i) {
                std::string first = token.substr(0, i);
                std::string second = token.substr(i);
                
                // If both parts are in the vocabulary, assume this is a valid merge rule
                if (encoder.count(first) && encoder.count(second)) {
                    // Use ID as priority - smaller ID means higher priority
                    merge_ranks[{first, second}] = id;
                }
            }
        }
    }
    
    /**
     * @brief Set merge priority
     * @param ranks new merge priority mapping
     */
    void set_merge_ranks(const std::unordered_map<std::pair<std::string, std::string>, size_t, PairHash>& ranks) {
        merge_ranks = ranks;
    }
    
    /**
     * @brief Encode text using BPE
     * @param text text to encode
     * @return encoded token IDs
     * 
     * This method uses BPE algorithm to encode the input text.
     * 1. First decompose the text into single byte tokens
     * 2. Use priority queue to merge adjacent tokens based on merge_ranks
     * 3. Convert the final tokens to corresponding IDs
     */
    std::vector<size_t> encode(const std::string& text) const override {
        if (text.empty()) {
            return {};
        }
        
        // Decompose the text into single character tokens
        std::vector<std::string> tokens;
        for (unsigned char c : text) {
            tokens.push_back(std::string(1, c));
        }
        
        // Use priority queue to execute BPE merging
        while (tokens.size() > 1) {
            // Build priority queue to select the highest priority merge pair
            using MergePair = std::pair<std::pair<std::string, std::string>, size_t>;
            MergeComparator comparator(merge_ranks);
            std::priority_queue<MergePair, std::vector<MergePair>, MergeComparator> merge_candidates(comparator);
            
            // Find all possible merge pairs
            for (size_t i = 0; i < tokens.size() - 1; ++i) {
                std::pair<std::string, std::string> pair = {tokens[i], tokens[i+1]};
                if (merge_ranks.count(pair)) {
                    merge_candidates.push({pair, i});
                }
            }
            
            // If there are no merge pairs, exit loop
            if (merge_candidates.empty()) {
                break;
            }
            
            // Execute the highest priority merge (the first in the priority queue)
            auto top_merge = merge_candidates.top();
            auto pair = top_merge.first;  // The token pair to merge
            size_t pos = top_merge.second;  // The position to merge
            
            // Merge tokens
            std::string merged_token = pair.first + pair.second;
            tokens[pos] = merged_token;
            tokens.erase(tokens.begin() + pos + 1);
        }
        
        // Convert tokens to IDs
        std::vector<size_t> ids;
        ids.reserve(tokens.size());
        for (const auto& token : tokens) {
            auto it = encoder.find(token);
            if (it != encoder.end()) {
                ids.push_back(it->second);
            }
            // Unknown tokens will be skipped
        }
        
        return ids;
    }
    
    /**
     * @brief Decode BPE tokens
     * @param tokens token IDs to decode
     * @return decoded text
     * 
     * This method converts encoded token IDs back to the original text.
     * Simply concatenate the token strings corresponding to each ID.
     */
    std::string decode(const std::vector<size_t>& tokens) const override {
        std::string result;
        result.reserve(tokens.size() * 2);
        
        for (size_t id : tokens) {
            auto it = decoder.find(id);
            if (it != decoder.end()) {
                result += it->second;
            }
            // Unknown IDs will be skipped
        }
        
        return result;
    }
    
    /**
     * @brief Load merge ranks from tiktoken format file
     * @param file_path path to tiktoken format file
     * @return shared pointer to created BPE tokenizer
     */
    static std::shared_ptr<BPETokenizer> load_from_tiktoken(const std::string& file_path) {
        return std::make_shared<BPETokenizer>(file_path);
    }
};

} // namespace humanus

#endif // HUMANUS_TOKENIZER_BPE_H
