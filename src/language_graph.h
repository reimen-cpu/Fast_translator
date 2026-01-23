#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

class LanguageGraph {
public:
    // Build graph from installed packages directory
    void BuildFromPackages(const std::string& packages_dir);
    
    // Find shortest path from source to target language
    // Returns empty vector if no path exists
    std::vector<std::string> FindPath(const std::string& from, const std::string& to);
    
    // Get all unique languages available
    std::set<std::string> GetAllLanguages() const;
    
    // Get package path for a specific language pair (if direct translation exists)
    std::string GetPackagePath(const std::string& from, const std::string& to) const;
    
    // Check if a direct translation exists
    bool HasDirectPath(const std::string& from, const std::string& to) const;

private:
    // Adjacency list: from_lang -> [to_langs]
    std::map<std::string, std::vector<std::string>> edges;
    
    // Map of (from, to) -> package_name for looking up models
    std::map<std::pair<std::string, std::string>, std::string> packages;
};
