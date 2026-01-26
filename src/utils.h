#pragma once
#include <string>

std::string get_clipboard_text();
void set_clipboard_text(const std::string &text);
void paste_clipboard();
void notify_user(const std::string &title, const std::string &message);
void show_log_dialog(const std::string &log_content);
std::string decode_html_entities(const std::string &text);
std::string translate_text(const std::string &text,
                           const std::string &model_dir);
