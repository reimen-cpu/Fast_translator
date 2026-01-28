#include "ollama.h"
#include "json.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

static const std::string OLLAMA_BASE_URL = "http://localhost:11434";

// Callback for libcurl to write response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                            std::string *userp) {
  size_t total_size = size * nmemb;
  userp->append(static_cast<char *>(contents), total_size);
  return total_size;
}

// Helper to make HTTP requests
static std::string http_request(const std::string &url,
                                const std::string &post_data = "") {
  CURL *curl = curl_easy_init();
  if (!curl) {
    return "";
  }

  std::string response;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                   120L); // 2 minute timeout for generation
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

  // For POST requests
  struct curl_slist *headers = nullptr;
  if (!post_data.empty()) {
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
  }

  CURLcode res = curl_easy_perform(curl);

  if (headers) {
    curl_slist_free_all(headers);
  }
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    std::cerr << "[Ollama] curl error: " << curl_easy_strerror(res)
              << std::endl;
    return "";
  }

  return response;
}

bool is_ollama_available() {
  std::string response = http_request(OLLAMA_BASE_URL + "/api/tags");
  return !response.empty();
}

std::vector<std::string> get_ollama_models() {
  std::vector<std::string> models;

  std::string response = http_request(OLLAMA_BASE_URL + "/api/tags");
  if (response.empty()) {
    return models;
  }

  try {
    json data = json::parse(response);
    if (data.contains("models") && data["models"].is_array()) {
      for (const auto &model : data["models"]) {
        if (model.contains("name")) {
          models.push_back(model["name"].get<std::string>());
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[Ollama] JSON parse error: " << e.what() << std::endl;
  }

  return models;
}

std::string query_ollama(const std::string &model, const std::string &prompt) {
  // System prompt for focused, non-verbose responses
  static const std::string SYSTEM_PROMPT =
      "Always answer in the user's language."
      "Absolute Mode. Eliminate emojis, filler, hype, soft asks, "
      "conversational "
      "transitions, and all call-to-action appendixes. Prioritize blunt, "
      "directive "
      "phrasing. Disable all behaviors optimizing for engagement, sentiment "
      "uplift, "
      "or interaction extension. Suppress emotional softening or continuation "
      "bias. "
      "Never mirror the user's diction, mood, or affect. No questions, no "
      "offers, "
      "no suggestions, no transitional phrasing. Terminate each reply "
      "immediately "
      "after delivering the requested material. No appendixes, no soft "
      "closures. "
      "Challenge assumptions with precision, offer unfamiliar perspectives. "
      "Be ruthless but respectful. Seek truth above comfort.";

  // Build request JSON
  json request;
  request["model"] = model;
  request["prompt"] = prompt;
  request["system"] = SYSTEM_PROMPT;
  request["stream"] = false;

  // Options removed to allow default Ollama behavior
  // request["options"] = ...;

  std::string post_data = request.dump();
  std::string response =
      http_request(OLLAMA_BASE_URL + "/api/generate", post_data);

  if (response.empty()) {
    return "Error: Failed to connect to Ollama. Is it running? (ollama serve)";
  }

  try {
    json data = json::parse(response);

    // Check for error
    if (data.contains("error")) {
      return "Error: " + data["error"].get<std::string>();
    }

    // Extract response text
    if (data.contains("response")) {
      return data["response"].get<std::string>();
    }

    return "Error: Unexpected response format from Ollama";
  } catch (const std::exception &e) {
    return "Error: Failed to parse Ollama response - " + std::string(e.what());
  }
}

std::string query_ollama_with_role(const std::string &model,
                                   const std::string &prompt,
                                   const std::string &role) {
  // System prompt for focused, non-verbose responses (same as query_ollama)
  static const std::string SYSTEM_PROMPT =
      "Absolute Mode. Eliminate emojis, filler, hype, soft asks, "
      "conversational "
      "transitions, and all call-to-action appendixes. Prioritize blunt, "
      "directive "
      "phrasing. Disable all behaviors optimizing for engagement, sentiment "
      "uplift, "
      "or interaction extension. Suppress emotional softening or continuation "
      "bias. "
      "Never mirror the user's diction, mood, or affect. No questions, no "
      "offers, "
      "no suggestions, no transitional phrasing. Terminate each reply "
      "immediately "
      "after delivering the requested material. No appendixes, no soft "
      "closures. "
      "Challenge assumptions with precision, offer unfamiliar perspectives. "
      "Be ruthless but respectful. Seek truth above comfort.";

  // Build request JSON
  json request;
  request["model"] = model;

  // Use role as system prompt if available to override hardcoded default
  if (!role.empty()) {
    request["prompt"] = prompt;
    request["system"] = role; // The role IS the system prompt
  } else {
    request["prompt"] = prompt; // No role, use hardcoded system
    request["system"] = SYSTEM_PROMPT;
  }

  request["stream"] = false;

  // Options removed to allow default Ollama behavior
  // request["options"] = ...;

  std::string post_data = request.dump();
  std::string response =
      http_request(OLLAMA_BASE_URL + "/api/generate", post_data);

  if (response.empty()) {
    return "Error: Failed to connect to Ollama. Is it running? (ollama serve)";
  }

  try {
    json data = json::parse(response);

    // Check for error
    if (data.contains("error")) {
      return "Error: " + data["error"].get<std::string>();
    }

    // Extract response text
    if (data.contains("response")) {
      return data["response"].get<std::string>();
    }

    return "Error: Unexpected response format from Ollama";
  } catch (const std::exception &e) {
    return "Error: Failed to parse Ollama response - " + std::string(e.what());
  }
}
