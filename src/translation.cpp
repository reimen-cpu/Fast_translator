#include "translation.h"
#include "tokenizer.h"
#include "tokenizer_bpe.h"
#include "tokenizer_sp.h"
#include <algorithm>
#include <ctranslate2/devices.h>
#include <ctranslate2/translator.h>
#include <thread>

struct ArgosTranslator::Impl {
  std::unique_ptr<Tokenizer> tokenizer;
  std::unique_ptr<ctranslate2::Translator> translator;
  ctranslate2::Device device_used;
};

ArgosTranslator::ArgosTranslator() : impl(std::make_unique<Impl>()) {}
ArgosTranslator::~ArgosTranslator() = default;

// Detect best available device: GPU if available, otherwise CPU
static ctranslate2::Device get_best_device() {
  try {
    int gpu_count = ctranslate2::get_gpu_count();
    if (gpu_count > 0) {
      std::cerr << "[Info] CUDA GPU detected (" << gpu_count
                << " device(s)), using GPU acceleration" << std::endl;
      return ctranslate2::Device::CUDA;
    }
  } catch (const std::exception &e) {
    std::cerr << "[Info] CUDA check failed: " << e.what() << std::endl;
  } catch (...) {
    // CUDA not available
  }
  std::cerr << "[Info] No GPU detected, using CPU" << std::endl;
  return ctranslate2::Device::CPU;
}

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
    // Detect best available device
    ctranslate2::Device device = get_best_device();
    impl->device_used = device;

    // Calculate safe thread count (75% of cores) - only used for CPU
    size_t num_threads = get_optimal_threads();

    // Create translator with automatic device selection
    impl->translator = std::make_unique<ctranslate2::Translator>(
        model_path, device, ctranslate2::ComputeType::DEFAULT,
        std::vector<int>{0}, // device_indices
        false,               // tensor_parallel
        ctranslate2::ReplicaPoolConfig{
            /* num_threads_per_replica */ num_threads,
            /* max_queued_batches */ 0,
            /* cpu_core_offset */ -1});

    if (device == ctranslate2::Device::CUDA) {
      std::cerr << "[Info] Model loaded on GPU" << std::endl;
    } else {
      std::cerr << "[Info] Using " << num_threads
                << " CPU threads for translation" << std::endl;
    }
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
