#pragma once
#include "tokenizer.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// Simple implementation of BPE application
class LegacyBPETokenizer : public Tokenizer {
public:
    bool load(const std::string& model_path) override;
    std::vector<std::string> encode(const std::string& text) override;
    std::string decode(const std::vector<std::string>& tokens) override;

private:
    struct PairHash {
        template <class T1, class T2>
        std::size_t operator()(const std::pair<T1, T2>& p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ h2;
        }
    };

    std::unordered_map<std::pair<std::string, std::string>, int, PairHash> bpe_ranks;
    
    std::vector<std::string> get_vocabulary(const std::string& word);
    std::vector<std::string> apply_bpe(const std::string& word);
};
