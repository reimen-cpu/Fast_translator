#pragma once
#include <string>
#include <vector>
#include <memory>

// Forward declarations to avoid heavy includes in header if possible,
// but for value types usually we just include.
// To keep compilation fast/safe, we might use PIMPL or just include.
// For now, simple include is fine if headers are available. 
// If not, we might hide them behind cpp.

class ArgosTranslator {
public:
    ArgosTranslator();
    ~ArgosTranslator();

    bool load_model(const std::string& model_path, const std::string& sp_model_path);
    std::string translate(const std::string& text);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
