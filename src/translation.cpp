#include "translation.h"
#include <ctranslate2/translator.h>
#include "tokenizer.h"
#include "tokenizer_sp.h"
#include "tokenizer_bpe.h"

struct ArgosTranslator::Impl {
    std::unique_ptr<Tokenizer> tokenizer;
    std::unique_ptr<ctranslate2::Translator> translator;
};

ArgosTranslator::ArgosTranslator() : impl(std::make_unique<Impl>()) {}
ArgosTranslator::~ArgosTranslator() = default;

bool ArgosTranslator::load_model(const std::string& model_path, const std::string& bpe_source_model) {
    if (bpe_source_model.find("sentencepiece.model") != std::string::npos) {
        impl->tokenizer = std::make_unique<SentencePieceTokenizer>();
    } else {
        // Assume legacy BPE if not sentencepiece
        impl->tokenizer = std::make_unique<LegacyBPETokenizer>();
    }

    if (!impl->tokenizer->load(bpe_source_model)) {
        std::cerr << "Failed to load tokenizer: " << bpe_source_model << std::endl;
        return false;
    }

    try {
        // ComputeType could be int8/float depending on model support
        impl->translator = std::make_unique<ctranslate2::Translator>(model_path, ctranslate2::Device::CPU);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load CTranslate2 model: " << e.what() << std::endl;
        return false;
    }

    return true;
}

std::string ArgosTranslator::translate(const std::string& text) {
    if (!impl->tokenizer || !impl->translator) {
        return "Error: Models not loaded.";
    }

    // 1. Tokenize
    std::vector<std::string> tokens = impl->tokenizer->encode(text);

    // 2. Translate
    ctranslate2::TranslationOptions options;
    // Simple greedy search
    options.beam_size = 1; 

    // Batch size 1
    const std::vector<std::vector<std::string>> batch = {tokens};
    const auto results = impl->translator->translate_batch(batch, options);
    
    if (results.empty()) return "";

    const auto& target_tokens = results[0].output();

    // 3. Detokenize
    std::string output = impl->tokenizer->decode(target_tokens);
    return output;
}
