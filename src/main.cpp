#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
// #include <ctranslate2/translator.h> // Hidden in translation.h
// #include <sentencepiece_processor.h>
#include "language_graph.h"
#include "ollama.h"
#include "response_processor.h"
#include "role_manager.h"
#include "translation.h"
#include "utils.h"
#ifdef _WIN32
#include <windows.h>
#define PATH_MAX MAX_PATH
#elif __APPLE__
#include <limits.h>
#include <mach-o/dyld.h>
#else
#include <limits.h> // Ensure limits is included
#include <linux/limits.h>
#include <unistd.h>
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

// Global logger to capture output
struct LogCapture {
  std::stringstream buffer;
  std::streambuf *old_cerr;

  LogCapture() { old_cerr = std::cerr.rdbuf(buffer.rdbuf()); }

  ~LogCapture() { std::cerr.rdbuf(old_cerr); }

  std::string get_logs() { return buffer.str(); }
};

int run_app(int argc, char *argv[]) {

  // Check for test/debug mode (--test "text" lang:lang)
  // This mode works without X11/clipboard for SSH debugging
  bool test_mode = false;
  std::string test_text;
  int arg_offset = 0;

  if (argc >= 2 &&
      (std::string(argv[1]) == "--test" || std::string(argv[1]) == "-t")) {
    test_mode = true;
    std::cerr << "[DEBUG] Test mode enabled - no clipboard access" << std::endl;
    std::cerr << "[DEBUG] Executable dir: " << get_executable_dir()
              << std::endl;

    // Determine if argv[2] is text or language code
    // Language codes contain ':' (e.g., es:en), text typically doesn't
    bool argv2_is_lang_code = false;
    if (argc >= 3) {
      std::string arg2 = argv[2];
      argv2_is_lang_code = (arg2.find(':') != std::string::npos);
    }

    if (argc >= 4 && !argv2_is_lang_code) {
      // Full form: --test "text" es:en
      test_text = argv[2];
      arg_offset = 2;
    } else if (argc >= 3 && argv2_is_lang_code) {
      // Stdin form: echo "text" | fast-translator --test es:en
      std::cerr << "[DEBUG] Reading text from stdin..." << std::endl;
      std::getline(std::cin, test_text);
      arg_offset = 1; // Language is at argv[2], so offset is 1
    } else if (argc >= 3) {
      // Assume argv[2] is text without language (use default en:es)
      test_text = argv[2];
      arg_offset = 2;
    } else {
      // Read from stdin with no language arg
      std::cerr << "[DEBUG] Reading text from stdin..." << std::endl;
      std::getline(std::cin, test_text);
    }

    if (test_text.empty()) {
      std::cerr << "[ERROR] No text provided for test mode" << std::endl;
      std::cerr << "Usage: fast-translator --test \"text to translate\" es:en"
                << std::endl;
      std::cerr << "   or: echo \"text\" | fast-translator --test es:en"
                << std::endl;
      return 1;
    }
    std::cerr << "[DEBUG] Input text: \"" << test_text << "\"" << std::endl;
  }

  // 1. Capture text from clipboard (or use test text)
  std::string input_text;
  if (test_mode) {
    input_text = test_text;
  } else {
    input_text = get_clipboard_text();
  }

  // Trim whitespace
  const auto str_begin = input_text.find_first_not_of(" \t\n\r");
  if (str_begin == std::string::npos) {
    if (test_mode) {
      std::cerr << "[ERROR] Input text is empty or whitespace" << std::endl;
    } else {
      notify_user("Argos", "Clipboard empty/whitespace");
    }
    return 1;
  }
  const auto str_end = input_text.find_last_not_of(" \t\n\r");
  input_text = input_text.substr(str_begin, str_end - str_begin + 1);

  std::cout << "Original: " << input_text << std::endl;

  // Check for Ollama mode (--ollama <model> or -o <model>)
  if (argc >= 3 &&
      (std::string(argv[1]) == "--ollama" || std::string(argv[1]) == "-o")) {
    std::string model = argv[2];
    std::cerr << "[DEBUG] Ollama mode detected. Model: " << model << std::endl;
    std::cout << "Ollama mode: using model " << model << std::endl;

    // Check for --role flag
    std::string role_prompt;
    std::string response_handler;

    for (int i = 3; i < argc - 1; i++) {
      if (std::string(argv[i]) == "--role" || std::string(argv[i]) == "-r") {
        std::string role_name = argv[i + 1];
        std::cerr << "[DEBUG] Role requested: " << role_name << std::endl;

        // Use RoleManager
        RoleManager::GetInstance().LoadRoles(); // Ensure roles are loaded
        RoleInfo role = RoleManager::GetInstance().GetRole(role_name);

        if (!role.name.empty()) {
          role_prompt = role.prompt;
          response_handler = role.response_handler;
          std::cerr << "[DEBUG] Role loaded. Handler: " << response_handler
                    << std::endl;
        } else {
          std::cerr << "[WARNING] Role '" << role_name
                    << "' not found in config" << std::endl;
        }
        break;
      }
    }

    std::cerr << "[DEBUG] Sending query to Ollama..." << std::endl;
    std::string response;

    if (!role_prompt.empty()) {
      response = query_ollama_with_role(model, input_text, role_prompt);
    } else {
      response = query_ollama(model, input_text);
    }
    std::cerr << "[DEBUG] Ollama response length: " << response.size()
              << std::endl;

    if (response.find("Error:") == 0) {
      std::cerr << "[ERROR] Ollama returned error: " << response << std::endl;
      notify_user("Ollama Error", response);
      return 1;
    }

    // Trim response
    const auto resp_begin = response.find_first_not_of(" \t\n\r");
    if (resp_begin != std::string::npos) {
      const auto resp_end = response.find_last_not_of(" \t\n\r");
      response = response.substr(resp_begin, resp_end - resp_begin + 1);
    }

    // Process response if handler is set
    if (!response_handler.empty()) {
      std::cerr << "[DEBUG] Post-processing response with handler: "
                << response_handler << std::endl;
      response = ResponseProcessor::Process(response, response_handler);
    }

    std::cout << "Response: " << response << std::endl;

    if (!test_mode) {
      std::cerr << "[DEBUG] Setting clipboard text..." << std::endl;
      set_clipboard_text(response);
      std::cerr << "[DEBUG] Pasting clipboard..." << std::endl;
      paste_clipboard();
    } else {
      std::cerr << "[DEBUG] Test mode - skipping clipboard write" << std::endl;
    }

    return 0;
  }

  // 2. Determine packages directory
  std::string exe_dir = get_executable_dir();
  std::string packages_dir = exe_dir + "/packages";

  if (test_mode) {
    std::cerr << "[DEBUG] exe_dir: " << exe_dir << std::endl;
    std::cerr << "[DEBUG] Initial packages_dir: " << packages_dir << std::endl;
    std::cerr << "[DEBUG] packages_dir exists: "
              << (std::filesystem::exists(packages_dir) ? "YES" : "NO")
              << std::endl;
  }

  if (!std::filesystem::exists(packages_dir)) {
    std::string dev_packages = exe_dir + "/../packages";
    if (std::filesystem::exists(dev_packages)) {
      packages_dir = dev_packages;
      if (test_mode) {
        std::cerr << "[DEBUG] Using dev_packages: " << packages_dir
                  << std::endl;
      }
    }
  }

  if (test_mode) {
    std::cerr << "[DEBUG] Final packages_dir: " << packages_dir << std::endl;
    // List packages
    if (std::filesystem::exists(packages_dir)) {
      std::cerr << "[DEBUG] Available packages:" << std::endl;
      for (const auto &entry :
           std::filesystem::directory_iterator(packages_dir)) {
        if (entry.is_directory()) {
          std::cerr << "[DEBUG]   - " << entry.path().filename().string()
                    << std::endl;
        }
      }
    }
  }

  // 3. Parse arguments for translation route
  std::vector<std::string> route; // Language codes in order

  // In test mode, language arg is after --test [text]
  int lang_arg_idx = test_mode ? (arg_offset + 1) : 1;

  if (test_mode) {
    std::cerr << "[DEBUG] Looking for language arg at index " << lang_arg_idx
              << std::endl;
    std::cerr << "[DEBUG] argc=" << argc << ", arg_offset=" << arg_offset
              << std::endl;
  }

  if (argc > lang_arg_idx) {
    std::string arg = argv[lang_arg_idx];
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
        if (i < route.size() - 1)
          std::cout << " -> ";
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
          if (i < route.size() - 1)
            std::cout << " -> ";
        }
        std::cout << std::endl;
      }
    } else {
      std::cerr << "Error: No translation path from " << route[0] << " to "
                << route[1] << std::endl;
      notify_user("Argos Error", "No translation path available");
      return 1;
    }
  }

  // 4. Execute translation chain
  std::string current_text = input_text;

  for (size_t i = 0; i < route.size() - 1; i++) {
    std::string from_lang = route[i];
    std::string to_lang = route[i + 1];

    std::cout << "Hop " << (i + 1) << ": " << from_lang << " -> " << to_lang
              << std::endl;

    // Find package for this hop
    LanguageGraph graph;
    graph.BuildFromPackages(packages_dir);
    std::string pkg_name = graph.GetPackagePath(from_lang, to_lang);

    std::cerr << "[DEBUG] Hop " << (i + 1) << " package: " << pkg_name
              << std::endl;

    if (pkg_name.empty()) {
      std::cerr << "Error: No package for " << from_lang << "->" << to_lang
                << std::endl;
      notify_user("Argos Error", "Missing translation package");
      return 1;
    }

    // Determine model paths
    std::string model_dir = packages_dir + "/" + pkg_name + "/model";
    std::string sp_model =
        packages_dir + "/" + pkg_name + "/sentencepiece.model";

    // Try .bpe.model as fallback
    if (!std::filesystem::exists(sp_model)) {
      sp_model = packages_dir + "/" + pkg_name + "/bpe.model";
    }

    std::cout << "  Loading: " << pkg_name << std::endl;
    std::cerr << "[DEBUG] Loading model from: " << model_dir << std::endl;

    ArgosTranslator translator;
    if (!translator.load_model(model_dir, sp_model)) {
      std::cerr << "[ERROR] Failed to load model: " << pkg_name << std::endl;
      notify_user("Argos Error", "Failed to load model: " + pkg_name);
      return 1;
    }

    std::cerr << "[DEBUG] Model loaded. Translating..." << std::endl;

    current_text = translator.translate(current_text);
    std::cerr << "[DEBUG] Raw translation length: " << current_text.size()
              << std::endl;

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
    std::cerr << "[DEBUG] Hop result: " << current_text << std::endl;
  }

  // 5. Post-process and output
  // Remove trailing punctuation and whitespace more aggressively
  while (!current_text.empty()) {
    char last = current_text.back();
    if (std::isspace(last) || last == '.' || last == ',' || last == ';' ||
        last == ':') {
      current_text.pop_back();
    } else {
      break;
    }
  }

  if (!current_text.empty()) {
    std::cout << "Final translation: " << current_text << std::endl;
    std::cerr << "[DEBUG] Final text: " << current_text << std::endl;

    if (test_mode) {
      std::cerr << "[DEBUG] Test mode - skipping clipboard write" << std::endl;
      std::cout << "\n=== TRANSLATION RESULT ===\n"
                << current_text << std::endl;
    } else {
      std::cerr << "[DEBUG] Setting clipboard..." << std::endl;
      set_clipboard_text(current_text);
      std::cerr << "[DEBUG] Pasting..." << std::endl;
      paste_clipboard();
    }
  } else {
    if (test_mode) {
      std::cerr << "[ERROR] Translation failed - empty result" << std::endl;
    } else {
      std::cerr << "[ERROR] Final text is empty" << std::endl;
      notify_user("Argos Error", "Translation failed");
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  // Capture logs for potential error dialog
  LogCapture log_capture;

  try {
    int result = run_app(argc, argv);
    if (result != 0) {
      std::cerr << log_capture.get_logs();
      show_log_dialog(log_capture.get_logs());
    } else {
      std::cerr << log_capture.get_logs();
    }
    return result;
  } catch (const std::exception &e) {
    std::cerr << log_capture.get_logs();
    std::cerr << "[CRITICAL] Exception: " << e.what() << std::endl;

    std::string crash_log =
        log_capture.get_logs() + "\n[CRITICAL] Exception: " + e.what();
    show_log_dialog(crash_log);
    return 1;
  } catch (...) {
    std::string crash_log =
        log_capture.get_logs() + "\n[CRITICAL] Unknown exception occurred";
    show_log_dialog(crash_log);
    return 1;
  }
}
