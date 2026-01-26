#include "utils.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// Helper to execute shell command and get output
std::string exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  auto deleter = [](FILE *f) {
    if (f)
      pclose(f);
  };
  std::unique_ptr<FILE, decltype(deleter)> pipe(popen(cmd, "r"), deleter);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

std::string get_clipboard_text() {
#ifdef __linux__
  // Check DISPLAY env
  const char *display = std::getenv("DISPLAY");
  std::cerr << "[DEBUG] get_clipboard_text: DISPLAY="
            << (display ? display : "(null)") << std::endl;

  try {
    std::string result = exec("xclip -selection primary -o 2>/dev/null");
    std::cerr << "[DEBUG] get_clipboard_text: got " << result.size() << " bytes"
              << std::endl;
    if (result.empty()) {
      std::cerr << "[DEBUG] Primary selection empty, trying clipboard..."
                << std::endl;
      result = exec("xclip -selection clipboard -o 2>/dev/null");
      std::cerr << "[DEBUG] Clipboard fallback got " << result.size()
                << " bytes" << std::endl;
    }
    return result;
  } catch (const std::exception &e) {
    std::cerr << "[ERROR] get_clipboard_text exception: " << e.what()
              << std::endl;
    return "";
  } catch (...) {
    std::cerr << "[ERROR] get_clipboard_text unknown exception" << std::endl;
    return "";
  }
#elif __APPLE__
  try {
    return exec("pbpaste");
  } catch (...) {
    return "";
  }
#elif _WIN32
  try {
    return exec("powershell.exe -command Get-Clipboard");
  } catch (...) {
    return "";
  }
#else
  return "";
#endif
}

void set_clipboard_text(const std::string &text) {
  std::cerr << "[DEBUG] set_clipboard_text: writing " << text.size() << " bytes"
            << std::endl;
#ifdef __linux__
  const char *display = std::getenv("DISPLAY");
  std::cerr << "[DEBUG] set_clipboard_text: DISPLAY="
            << (display ? display : "(null)") << std::endl;

  FILE *pipe = popen("xclip -selection clipboard -i 2>&1", "w");
  if (pipe) {
    size_t written = fwrite(text.c_str(), 1, text.size(), pipe);
    int result = pclose(pipe);
    std::cerr << "[DEBUG] set_clipboard_text: wrote " << written
              << " bytes, pclose=" << result << std::endl;
  } else {
    std::cerr << "[ERROR] set_clipboard_text: popen failed" << std::endl;
  }
#elif __APPLE__
  FILE *pipe = popen("pbcopy", "w");
  if (pipe) {
    fwrite(text.c_str(), 1, text.size(), pipe);
    pclose(pipe);
  }
#elif _WIN32
  FILE *pipe = popen("clip", "w");
  if (pipe) {
    fwrite(text.c_str(), 1, text.size(), pipe);
    pclose(pipe);
  }
#endif
}

void paste_clipboard() {
  std::cerr << "[DEBUG] paste_clipboard: starting" << std::endl;
  [[maybe_unused]] int ret;
#ifdef __linux__
  const char *display = std::getenv("DISPLAY");
  std::cerr << "[DEBUG] paste_clipboard: DISPLAY="
            << (display ? display : "(null)") << std::endl;

  // Try xdotool
  ret = system("xdotool version >/dev/null 2>&1");
  if (ret != 0) {
    std::cerr << "[ERROR] paste_clipboard: xdotool not found" << std::endl;
  }

  std::cerr << "[DEBUG] paste_clipboard: executing xdotool" << std::endl;
  ret = system("sleep 0.2 && xdotool key --clearmodifiers ctrl+v 2>&1");
  std::cerr << "[DEBUG] paste_clipboard: xdotool returned " << ret << std::endl;
#elif __APPLE__
  ret = system("osascript -e 'tell application \"System Events\" to keystroke "
               "\"v\" using command down'");
#elif _WIN32
#endif
}

void notify_user(const std::string &title, const std::string &message) {
  std::cerr << "[DEBUG] notify_user: " << title << " - " << message
            << std::endl;
  [[maybe_unused]] int ret;
#ifdef __linux__
  std::string cmd = "notify-send \"" + title + "\" \"" + message + "\" 2>&1";
  ret = system(cmd.c_str());
  std::cerr << "[DEBUG] notify_user: notify-send returned " << ret << std::endl;
#elif __APPLE__
  std::string cmd = "osascript -e 'display notification \"" + message +
                    "\" with title \"" + title + "\"'";
  ret = system(cmd.c_str());
#elif _WIN32
#endif
}

void show_log_dialog(const std::string &log_content) {
#ifdef __linux__
  // Use zenity to show log in a scrolling window
  // Check if zenity is available
  if (system("which zenity >/dev/null 2>&1") == 0) {
    // Write log to temp file to safely pass to zenity
    std::string tmp_file = "/tmp/fast_translator_error.log";
    FILE *f = fopen(tmp_file.c_str(), "w");
    if (f) {
      fwrite(log_content.c_str(), 1, log_content.size(), f);
      fclose(f);

      std::string cmd = "zenity --text-info --title=\"Fast Translator Error\" "
                        "--filename=\"" +
                        tmp_file + "\" --width=600 --height=400 2>/dev/null";
      system(cmd.c_str());
    }
  } else {
    // Fallback to notify-send if zenity not found
    notify_user("Fast Translator Error",
                "Check logs at /tmp/fast_translator_debug.log");
  }
#elif _WIN32
#endif
}

// Placeholder for translation until we integrate libraries
std::string translate_text(const std::string &text,
                           const std::string &model_dir) {
  // TODO: Integrate CTranslate2 and SentencePiece
  return "TRANSLATED: " + text;
}

std::string decode_html_entities(const std::string &text) {
  std::string result = text;
  // Replace common entities
  // Ordered to avoid double replacement (though &amp; usually should be first
  // if we supported recursive, but here it's single pass mostly)

  // Simple naive replacement
  // Using a loop to find and replace
  struct Entity {
    const char *pattern;
    const char *replacement;
  };
  Entity entities[] = {{"&apos;", "'"},
                       {"&quot;", "\""},
                       {"&amp;", "&"},
                       {"&lt;", "<"},
                       {"&gt;", ">"}};

  for (const auto &ent : entities) {
    size_t pos = 0;
    while ((pos = result.find(ent.pattern, pos)) != std::string::npos) {
      result.replace(pos, std::strlen(ent.pattern), ent.replacement);
      pos += std::strlen(ent.replacement);
    }
  }
  return result;
}
