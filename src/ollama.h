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

// Query Ollama with a prompt and optional role instruction
// The role is prepended to the user's prompt as an instruction
// Returns error message on failure (starts with "Error:")
std::string query_ollama_with_role(const std::string &model,
                                   const std::string &prompt,
                                   const std::string &role);

// Check if Ollama is running
bool is_ollama_available();
