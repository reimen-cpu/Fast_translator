#include "utils.h"
#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <cstring>

// Helper to execute shell command and get output
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string get_clipboard_text() {
    // Emulate: xclip -selection primary -o 2>/dev/null
    try {
        return exec("xclip -selection primary -o 2>/dev/null");
    } catch (...) {
        return "";
    }
}

void set_clipboard_text(const std::string& text) {
    // Emulate: echo -n "$TRADUCCION" | xclip -selection clipboard -i
    // Caution with shell injection if text has quotes. Ideally use pipe properly.
    // For simplicity in this step, we'll try basic popen or just system with careful quoting?
    // Actually, popen w/ "w" is better to write TO the process.
    
    FILE* pipe = popen("xclip -selection clipboard -i", "w");
    if (pipe) {
        fwrite(text.c_str(), 1, text.size(), pipe);
        pclose(pipe);
    }
}

void paste_clipboard() {
    // Emulate: xdotool key --clearmodifiers ctrl+v
    // sleep 0.2
    system("sleep 0.2 && xdotool key --clearmodifiers ctrl+v");
}

void notify_user(const std::string& title, const std::string& message) {
    // Emulate: notify-send "Title" "Message"
    std::string cmd = "notify-send \"" + title + "\" \"" + message + "\"";
    system(cmd.c_str());
}

// Placeholder for translation until we integrate libraries
std::string translate_text(const std::string& text, const std::string& model_dir) {
    // TODO: Integrate CTranslate2 and SentencePiece
    return "TRANSLATED: " + text; 
}

std::string decode_html_entities(const std::string& text) {
    std::string result = text;
    // Replace common entities
    // Ordered to avoid double replacement (though &amp; usually should be first if we supported recursive, 
    // but here it's single pass mostly)
    
    // Simple naive replacement
    // Using a loop to find and replace
    struct Entity { const char* pattern; const char* replacement; };
    Entity entities[] = {
        {"&apos;", "'"},
        {"&quot;", "\""},
        {"&amp;", "&"},
        {"&lt;", "<"},
        {"&gt;", ">"}
    };

    for (const auto& ent : entities) {
        size_t pos = 0;
        while ((pos = result.find(ent.pattern, pos)) != std::string::npos) {
            result.replace(pos, std::strlen(ent.pattern), ent.replacement);
            pos += std::strlen(ent.replacement);
        }
    }
    return result;
}
