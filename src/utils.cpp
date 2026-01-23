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
  // Emulate: xclip -selection primary -o 2>/dev/null
  try {
    return exec("xclip -selection primary -o 2>/dev/null");
  } catch (...) {
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
#ifdef __linux__
  FILE *pipe = popen("xclip -selection clipboard -i", "w");
  if (pipe) {
    fwrite(text.c_str(), 1, text.size(), pipe);
    pclose(pipe);
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
  [[maybe_unused]] int ret;
#ifdef __linux__
  ret = system("sleep 0.2 && xdotool key --clearmodifiers ctrl+v");
#elif __APPLE__
  // osascript to pres command+v?
  ret = system("osascript -e 'tell application \"System Events\" to keystroke "
               "\"v\" using command down'");
#elif _WIN32
// Windows is harder via system command. Maybe SendKeys via VBS or Powershell?
// Using a simple powershell SendKeys for now
// system("powershell.exe -c \"Add-Type -AssemblyName System.Windows.Forms;
// [System.Windows.Forms.SendKeys]::SendWait('^v')\"");
#endif
}

void notify_user(const std::string &title, const std::string &message) {
  [[maybe_unused]] int ret;
#ifdef __linux__
  std::string cmd = "notify-send \"" + title + "\" \"" + message + "\"";
  ret = system(cmd.c_str());
#elif __APPLE__
  std::string cmd = "osascript -e 'display notification \"" + message +
                    "\" with title \"" + title + "\"'";
  ret = system(cmd.c_str());
#elif _WIN32
// Powershell notification logic is complex, skipping for brevity or use msg *
// std::string cmd = "msg * " + message;
// system(cmd.c_str());
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
