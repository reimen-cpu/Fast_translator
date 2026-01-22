#pragma once
#include <string>
#include <vector>

class Tokenizer {
public:
    virtual ~Tokenizer() = default;
    virtual bool load(const std::string& model_path) = 0;
    virtual std::vector<std::string> encode(const std::string& text) = 0;
    virtual std::string decode(const std::vector<std::string>& tokens) = 0;
};
