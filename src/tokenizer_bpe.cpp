#include "tokenizer_bpe.h"
#include <iterator>

bool LegacyBPETokenizer::load(const std::string& model_path) {
    std::ifstream file(model_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open BPE model: " << model_path << std::endl;
        return false;
    }

    std::string line;
    // Skip version line if present
    if (std::getline(file, line)) {
        if (line.rfind("#version", 0) != 0) {
           // Not a version line, process it
           // Reset to beginning? Or just process. 
           // Usually first line is version.
           if (line.find("#") == 0) { 
               // comment, ignore
           } else {
               // process line
               std::stringstream ss(line);
               std::string p1, p2;
               ss >> p1 >> p2;
               if (!p1.empty() && !p2.empty()) bpe_ranks[{p1, p2}] = 0;
           }
        }
    }

    int rank = 1;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string p1, p2;
        ss >> p1 >> p2; // Read first two tokens
        if (!p1.empty() && !p2.empty()) {
            bpe_ranks[{p1, p2}] = rank++;
        }
    }
    return true;
}

std::vector<std::string> LegacyBPETokenizer::encode(const std::string& text) {
    std::vector<std::string> bpe_tokens;
    std::stringstream ss(text);
    std::string word;
    
    while (ss >> word) {
        // Simple pre-tokenization: just split by space.
        // TODO: Handle punctuation splitting loop if needed (Arcos usually does this).
        // For now, assume pure whitespace splitting.
        
        std::vector<std::string> word_bpe = apply_bpe(word);
        
        // Post-process: </w> -> end of word, others -> @@
        for (auto& t : word_bpe) {
            if (t.length() >= 4 && t.substr(t.length() - 4) == "</w>") {
                t = t.substr(0, t.length() - 4); // Remove </w>
            } else {
                t += "@@"; // Append @@
            }
        }

        bpe_tokens.insert(bpe_tokens.end(), word_bpe.begin(), word_bpe.end());
    }
    
    return bpe_tokens;
}

std::string LegacyBPETokenizer::decode(const std::vector<std::string>& tokens) {
    std::string text;
    for (const auto& token : tokens) {
        if (token.length() >= 2 && token.substr(token.length() - 2) == "@@") {
            text += token.substr(0, token.length() - 2);
        } else {
            text += token + " ";
        }
    }
    // Trim trailing space
    if (!text.empty() && text.back() == ' ') text.pop_back();
    return text;
}

std::vector<std::string> get_utf8_chars(const std::string& str) {
    std::vector<std::string> chars;
    for (size_t i = 0; i < str.length();) {
        int c = (unsigned char)str[i];
        int len = 1;
        if ((c & 0x80) == 0) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        
        if (i + len > str.length()) len = 1; // Safety fallback
        chars.push_back(str.substr(i, len));
        i += len;
    }
    return chars;
}

std::vector<std::string> LegacyBPETokenizer::apply_bpe(const std::string& word) {
    std::vector<std::string> split_word = get_utf8_chars(word);
    
    if (split_word.empty()) return {};
    split_word.back() += "</w>";

    while (split_word.size() > 1) {
        // Find best pair
        int min_rank = -1;
        std::pair<std::string, std::string> best_pair;
        int best_idx = -1;

        for (size_t i = 0; i < split_word.size() - 1; ++i) {
            std::pair<std::string, std::string> pair = {split_word[i], split_word[i+1]};
            if (bpe_ranks.count(pair)) {
                int rank = bpe_ranks[pair];
                if (min_rank == -1 || rank < min_rank) {
                    min_rank = rank;
                    best_pair = pair;
                    best_idx = i;
                }
            }
        }

        if (min_rank == -1) break; // No more merges possible

        // Apply merge
        std::string merged = best_pair.first + best_pair.second;
        
        // We need to merge all occurrences? Usually BPE applies one by one or all?
        // Standard BPE applies the best merge globally or locally? 
        // "Iteratively, replace the most frequent pair..." 
        // In local word context, we just replace the first logic or all logic.
        // Let's create a new vector
        std::vector<std::string> new_split;
        for (size_t i = 0; i < split_word.size(); ++i) {
            if (i < split_word.size() - 1 && 
                split_word[i] == best_pair.first && 
                split_word[i+1] == best_pair.second) {
                new_split.push_back(merged);
                i++; // Skip next
            } else {
                new_split.push_back(split_word[i]);
            }
        }
        split_word = new_split;
    }

    return split_word;
}
