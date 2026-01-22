#include <iostream>
#include <string>
#include <vector>
#include <memory>
// #include <ctranslate2/translator.h> // Hidden in translation.h
// #include <sentencepiece_processor.h>
#include "utils.h"
#include "translation.h"
#include <unistd.h>
#include <linux/limits.h>
#include <fstream>
#include <filesystem>

std::string get_executable_dir() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        std::string path(result, count); 
        return path.substr(0, path.find_last_of("/"));
    }
    return "";
}

int main(int argc, char* argv[]) {
    // 1. Capture text from clipboard
    std::string input_text = get_clipboard_text();
    
    // Trim whitespace
    const auto str_begin = input_text.find_first_not_of(" \t\n\r");
    if (str_begin == std::string::npos) {
         notify_user("Argos", "Clipboard empty/whitespace");
         return 0;
    }
    const auto str_end = input_text.find_last_not_of(" \t\n\r");
    input_text = input_text.substr(str_begin, str_end - str_begin + 1);

    std::cout << "Original: " << input_text << std::endl;

    // 2. Load Models
    std::string exe_dir = get_executable_dir();
    // Assuming 'build' is a subdirectory, and 'packages' is at the project root.
    // If the executable is in 'build/', we need to go up one level to find 'packages/'.
    // Or we can assume the user installs it properly.
    // Let's assume the standard development layout:
    // root/
    //   build/argos_native_cpp
    //   packages/
    
    // Check if 'packages' exists in exe_dir (deployment case)
    // or in exe_dir/../packages (dev case)
    
    std::string base_dir = exe_dir + "/.."; // Default to dev layout (build/..)
    
    // Simple check: if packages exists in exe_dir, use it.
    std::ifstream check_deployment(exe_dir + "/packages");
    if (check_deployment.good() || std::filesystem::exists(exe_dir + "/packages")) {
         base_dir = exe_dir;
    }

    // Default: EN -> ES
    std::string model_dir = base_dir + "/packages/en_es/model"; 
    std::string sp_model = base_dir + "/packages/en_es/sentencepiece.model";

    // Simple arg to switch to ES -> EN
    // Usage: ./argos_native_cpp es
    if (argc > 1 && std::string(argv[1]) == "es") {
        model_dir = base_dir + "/packages/translate-es_en-1_9/model"; 
        sp_model = base_dir + "/packages/translate-es_en-1_9/bpe.model"; 
        std::cout << "Mode: ES -> EN" << std::endl;
    } else {
        std::cout << "Mode: EN -> ES (Default)" << std::endl;
    }

    std::cout << "Loading models from: " << model_dir << std::endl;

    ArgosTranslator translator;
    // notify_user("Argos", "Loading models..."); // Removed per user request
    
    if (!translator.load_model(model_dir, sp_model)) {
        notify_user("Argos Error", "Failed to load models");
        return 1;
    }

    // 3. Translate
    std::string translated_text = translator.translate(input_text);
    
    // Fix HTML entities (e.g. &apos; -> ')
    translated_text = decode_html_entities(translated_text);

    // Remove trailing period/whitespace if present (User Request)
    while (!translated_text.empty() && (std::isspace(translated_text.back()) || translated_text.back() == '.')) {
        translated_text.pop_back();
    }

    // 4. Paste/Output
    if (!translated_text.empty()) {
        std::cout << "Translated: " << translated_text << std::endl;
        set_clipboard_text(translated_text);
        paste_clipboard();
        // notify_user("Argos", "Translated!"); // Removed per user request
    } else {
        notify_user("Argos Error", "Translation failed");
    }

    return 0;
}
