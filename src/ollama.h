#pragma once
#include <string>
#include <vector>

// Ollama API client for local LLM inference
// Connects to http://localhost:11434

// Get list of available models from Ollama
std::vector<std::string> get_ollama_models();

// Query Ollama with a prompt and return the response
// Returns error message on failure (starts with "Error:")
std::string query_ollama(const std::string &model, const std::string &prompt);

// Check if Ollama is running
bool is_ollama_available();
