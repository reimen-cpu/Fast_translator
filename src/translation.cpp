#include "translation.h"
#include "tokenizer.h"
#include "tokenizer_bpe.h"
#include "tokenizer_sp.h"
#include <algorithm>
#include <ctranslate2/translator.h>
#include <thread>

struct ArgosTranslator::Impl {
  std::unique_ptr<Tokenizer> tokenizer;
  std::unique_ptr<ctranslate2::Translator> translator;
};

ArgosTranslator::ArgosTranslator() : impl(std::make_unique<Impl>()) {}
ArgosTranslator::~ArgosTranslator() = default;

// Calculate optimal thread count: use ~75% of available cores, minimum 1
static size_t get_optimal_threads() {
  unsigned int hw_threads = std::thread::hardware_concurrency();
  if (hw_threads == 0)
    hw_threads = 4; // Fallback if detection fails

  // Use 75% of cores, but at least 1 and leave at least 1 for system
  size_t optimal = std::max(1u, static_cast<unsigned int>(hw_threads * 0.75));
  return std::min(optimal, static_cast<size_t>(hw_threads - 1));
}

bool ArgosTranslator::load_model(const std::string &model_path,
                                 const std::string &bpe_source_model) {
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
    // Calculate safe thread count (75% of cores)
    size_t num_threads = get_optimal_threads();

    // Create translator with controlled thread usage
    // inter_threads=1: single translation at a time
    // intra_threads=num_threads: threads per translation operation
    impl->translator = std::make_unique<ctranslate2::Translator>(
        model_path, ctranslate2::Device::CPU, ctranslate2::ComputeType::DEFAULT,
        std::vector<int>{0}, // device_indices
        false,               // tensor_parallel
        ctranslate2::ReplicaPoolConfig{
            /* num_threads_per_replica */ num_threads,
            /* max_queued_batches */ 0,
            /* cpu_core_offset */ -1});

    std::cerr << "[Info] Using " << num_threads
              << " CPU threads for translation" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Failed to load CTranslate2 model: " << e.what() << std::endl;
    return false;
  }

  return true;
}

std::string ArgosTranslator::translate(const std::string &text) {
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

  if (results.empty())
    return "";

  const auto &target_tokens = results[0].output();

  // 3. Detokenize
  std::string output = impl->tokenizer->decode(target_tokens);
  return output;
}
