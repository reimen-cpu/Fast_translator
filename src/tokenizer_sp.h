#pragma once
#include "tokenizer.h"
#include <sentencepiece_processor.h>
#include <memory>
#include <iostream>

class SentencePieceTokenizer : public Tokenizer {
public:
    bool load(const std::string& model_path) override {
        processor = std::make_unique<sentencepiece::SentencePieceProcessor>();
        const auto status = processor->Load(model_path);
        if (!status.ok()) {
            std::cerr << "Failed to load SP model: " << model_path << " (" << status.ToString() << ")" << std::endl;
            return false;
        }
        return true;
    }

    std::vector<std::string> encode(const std::string& text) override {
        std::vector<std::string> tokens;
        if (processor) {
            processor->Encode(text, &tokens);
        }
        return tokens;
    }

    std::string decode(const std::vector<std::string>& tokens) override {
        std::string text;
        if (processor) {
            processor->Decode(tokens, &text);
        }
        return text;
    }

private:
    std::unique_ptr<sentencepiece::SentencePieceProcessor> processor;
};
