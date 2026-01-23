#include <iostream>
#include <string>
#include <vector>
#include <memory>
// #include <ctranslate2/translator.h> // Hidden in translation.h
// #include <sentencepiece_processor.h>
#include "utils.h"
#include "translation.h"
#include "language_graph.h"
#ifdef _WIN32
    #include <windows.h>
    #define PATH_MAX MAX_PATH
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <limits.h>
#else
    #include <unistd.h>
    #include <linux/limits.h>
    #include <limits.h> // Ensure limits is included
#endif

#include <filesystem>
#include <fstream>

std::string get_executable_dir() {
    char buf[PATH_MAX];
    std::string path;
    
    #ifdef _WIN32
        // Windows
        GetModuleFileNameA(NULL, buf, PATH_MAX);
        path = std::string(buf);
        path = path.substr(0, path.find_last_of("\\"));
    #elif __APPLE__
        // macOS
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0) {
            path = std::string(buf);
            path = path.substr(0, path.find_last_of("/"));
        }
    #else
        // Linux
        ssize_t count = readlink("/proc/self/exe", buf, PATH_MAX);
        if (count != -1) {
            path = std::string(buf, count);
            path = path.substr(0, path.find_last_of("/"));
        }
    #endif
    
    return path;
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

    // 2. Determine packages directory
    std::string exe_dir = get_executable_dir();
    std::string packages_dir = exe_dir + "/packages";
    
    if (!std::filesystem::exists(packages_dir)) {
         std::string dev_packages = exe_dir + "/../packages";
         if (std::filesystem::exists(dev_packages)) {
             packages_dir = dev_packages;
         }
    }

    // 3. Parse arguments for translation route
    std::vector<std::string> route; // Language codes in order
    
    if (argc > 1) {
        std::string arg = argv[1];
        
        // Check for colon-separated chain syntax (from:to or from:mid:to)
        if (arg.find(':') != std::string::npos) {
            // Parse explicit chain
            size_t pos = 0;
            while (pos < arg.length()) {
                size_t next_colon = arg.find(':', pos);
                if (next_colon == std::string::npos) {
                    route.push_back(arg.substr(pos));
                    break;
                } else {
                    route.push_back(arg.substr(pos, next_colon - pos));
                    pos = next_colon + 1;
                }
            }
            std::cout << "Chain mode: ";
            for (size_t i = 0; i < route.size(); i++) {
                std::cout << route[i];
                if (i < route.size() - 1) std::cout << " -> ";
            }
            std::cout << std::endl;
        } else {
            // Legacy single-arg mode (backward compatibility)
            if (arg == "es") {
                route = {"es", "en"};
            } else {
                // Try to interpret as "from_code" (from -> en)
                route = {arg, "en"};
            }
        }
    } else {
        // Default: EN -> ES
        route = {"en", "es"};
    }

    // If only source and target specified, find path automatically
    if (route.size() == 2) {
        LanguageGraph graph;
        graph.BuildFromPackages(packages_dir);
        
        std::vector<std::string> path = graph.FindPath(route[0], route[1]);
        if (!path.empty()) {
            route = path;
            if (route.size() > 2) {
                std::cout << "Auto-route (" << (route.size() - 1) << " hops): ";
                for (size_t i = 0; i < route.size(); i++) {
                    std::cout << route[i];
                    if (i < route.size() - 1) std::cout << " -> ";
                }
                std::cout << std::endl;
            }
        } else {
            std::cerr << "Error: No translation path from " << route[0] << " to " << route[1] << std::endl;
            notify_user("Argos Error", "No translation path available");
            return 1;
        }
    }

    // 4. Execute translation chain
    std::string current_text = input_text;
    
    for (size_t i = 0; i < route.size() - 1; i++) {
        std::string from_lang = route[i];
        std::string to_lang = route[i + 1];
        
        std::cout << "Hop " << (i + 1) << ": " << from_lang << " -> " << to_lang << std::endl;
        
        // Find package for this hop
        LanguageGraph graph;
        graph.BuildFromPackages(packages_dir);
        std::string pkg_name = graph.GetPackagePath(from_lang, to_lang);
        
        if (pkg_name.empty()) {
            std::cerr << "Error: No package for " << from_lang << "->" << to_lang << std::endl;
            notify_user("Argos Error", "Missing translation package");
            return 1;
        }
        
        // Determine model paths
        std::string model_dir = packages_dir + "/" + pkg_name + "/model";
        std::string sp_model = packages_dir + "/" + pkg_name + "/sentencepiece.model";
        
        // Try .bpe.model as fallback
        if (!std::filesystem::exists(sp_model)) {
            sp_model = packages_dir + "/" + pkg_name + "/bpe.model";
        }
        
        std::cout << "  Loading: " << pkg_name << std::endl;
        
        ArgosTranslator translator;
        if (!translator.load_model(model_dir, sp_model)) {
            notify_user("Argos Error", "Failed to load model: " + pkg_name);
            return 1;
        }
        
        current_text = translator.translate(current_text);
        current_text = decode_html_entities(current_text);
        
        // Clean SentencePiece artifacts (▁ = U+2581, UTF-8: E2 96 81)
        std::string sp_marker = "\xE2\x96\x81"; // ▁
        // Replace internal markers with spaces
        size_t pos = 0;
        while ((pos = current_text.find(sp_marker, pos)) != std::string::npos) {
            if (pos == 0) {
                // Remove leading marker
                current_text.erase(pos, sp_marker.length());
            } else {
                // Replace with space
                current_text.replace(pos, sp_marker.length(), " ");
                pos += 1;
            }
        }
        
        std::cout << "  Result: " << current_text << std::endl;
    }

    // 5. Post-process and output
    // Remove trailing punctuation and whitespace more aggressively
    while (!current_text.empty()) {
        char last = current_text.back();
        if (std::isspace(last) || last == '.' || last == ',' || last == ';' || last == ':') {
            current_text.pop_back();
        } else {
            break;
        }
    }

    if (!current_text.empty()) {
        std::cout << "Final translation: " << current_text << std::endl;
        set_clipboard_text(current_text);
        paste_clipboard();
    } else {
        notify_user("Argos Error", "Translation failed");
    }

    return 0;
}
