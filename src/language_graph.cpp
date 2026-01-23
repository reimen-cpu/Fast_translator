#include "language_graph.h"
#include <filesystem>
#include <queue>
#include <algorithm>

void LanguageGraph::BuildFromPackages(const std::string& packages_dir) {
    edges.clear();
    packages.clear();
    
    if (!std::filesystem::exists(packages_dir)) {
        return;
    }
    
    // Scan all directories in packages folder
    for (const auto& entry : std::filesystem::directory_iterator(packages_dir)) {
        if (!entry.is_directory()) continue;
        
        std::string pkg_name = entry.path().filename().string();
        
        // Parse package name to extract language codes
        // Expected format: translate-XX_YY-version or XX_YY
        std::string from_code, to_code;
        
        size_t translate_pos = pkg_name.find("translate-");
        if (translate_pos != std::string::npos) {
            std::string lang_part = pkg_name.substr(translate_pos + 10);
            size_t underscore = lang_part.find('_');
            if (underscore != std::string::npos) {
                from_code = lang_part.substr(0, underscore);
                size_t dash = lang_part.find('-', underscore);
                if (dash != std::string::npos) {
                    to_code = lang_part.substr(underscore + 1, dash - underscore - 1);
                } else {
                    to_code = lang_part.substr(underscore + 1);
                }
            }
        } else {
            // Try simple XX_YY pattern
            size_t underscore = pkg_name.find('_');
            if (underscore != std::string::npos) {
                from_code = pkg_name.substr(0, underscore);
                to_code = pkg_name.substr(underscore + 1);
            }
        }
        
        if (!from_code.empty() && !to_code.empty()) {
            // Add edge to graph
            edges[from_code].push_back(to_code);
            packages[{from_code, to_code}] = pkg_name;
        }
    }
}

std::vector<std::string> LanguageGraph::FindPath(const std::string& from, const std::string& to) {
    if (from == to) {
        return {from};
    }
    
    // BFS to find shortest path
    std::queue<std::string> queue;
    std::map<std::string, std::string> parent; // child -> parent
    std::set<std::string> visited;
    
    queue.push(from);
    visited.insert(from);
    parent[from] = "";
    
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        
        if (current == to) {
            // Reconstruct path
            std::vector<std::string> path;
            std::string node = to;
            while (!node.empty()) {
                path.push_back(node);
                node = parent[node];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        
        // Explore neighbors
        if (edges.count(current)) {
            for (const auto& neighbor : edges[current]) {
                if (visited.find(neighbor) == visited.end()) {
                    visited.insert(neighbor);
                    parent[neighbor] = current;
                    queue.push(neighbor);
                }
            }
        }
    }
    
    // No path found
    return {};
}

std::set<std::string> LanguageGraph::GetAllLanguages() const {
    std::set<std::string> languages;
    
    for (const auto& [from, to_list] : edges) {
        languages.insert(from);
        for (const auto& to : to_list) {
            languages.insert(to);
        }
    }
    
    return languages;
}

std::string LanguageGraph::GetPackagePath(const std::string& from, const std::string& to) const {
    auto it = packages.find({from, to});
    if (it != packages.end()) {
        return it->second;
    }
    return "";
}

bool LanguageGraph::HasDirectPath(const std::string& from, const std::string& to) const {
    return packages.count({from, to}) > 0;
}
