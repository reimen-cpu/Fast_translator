#include "response_processor.h"
#include "json.hpp"
#include <iostream>
#include <sstream>

using json = nlohmann::json;

std::string ResponseProcessor::Process(const std::string &rawResponse,
                                       const std::string &handler) {
  if (handler == "json_to_markdown") {
    return JsonToMarkdown(rawResponse);
  }
  // Default: return raw response if no matching handler
  return rawResponse;
}

std::string ResponseProcessor::JsonToMarkdown(const std::string &jsonStr) {
  try {
    // Clean up markdown blocks if present (e.g. ```json ... ```)
    std::string cleanJson = jsonStr;
    size_t start = cleanJson.find("```json");
    if (start != std::string::npos) {
      cleanJson = cleanJson.substr(start + 7);
    } else {
      start = cleanJson.find("```"); // generic block
      if (start != std::string::npos) {
        cleanJson = cleanJson.substr(start + 3);
      }
    }

    // Find closing block if we found an opening one
    if (start != std::string::npos) {
      size_t end = cleanJson.find("```");
      if (end != std::string::npos) {
        cleanJson = cleanJson.substr(0, end);
      }
    }

    // Additional cleanup for leading/trailing whitespace or random characters
    size_t firstBrace = cleanJson.find('{');
    if (firstBrace != std::string::npos) {
      cleanJson = cleanJson.substr(firstBrace);
    }
    size_t lastBrace = cleanJson.rfind('}');
    if (lastBrace != std::string::npos) {
      cleanJson = cleanJson.substr(0, lastBrace + 1);
    }

    json j = json::parse(cleanJson);
    std::stringstream ss;

    // Headless API Schema Processing

    // 1. Meta Analysis
    if (j.contains("meta_analysis")) {
      auto meta = j["meta_analysis"];
      if (meta.contains("intent")) {
        ss << "### Intent\n" << meta["intent"].get<std::string>() << "\n\n";
      }
    }

    // 2. Optimized Prompt (The core value)
    if (j.contains("optimized_prompt")) {
      ss << "### Optimized Prompt\n"
         << "```text\n"
         << j["optimized_prompt"].get<std::string>() << "\n```\n\n";
    }

    // 3. Prompt Components (Details)
    if (j.contains("prompt_components")) {
      auto comps = j["prompt_components"];
      ss << "<details><summary>Prompt Details</summary>\n\n";

      if (comps.contains("role")) {
        ss << "**Role:** " << comps["role"].get<std::string>() << "\n\n";
      }
      if (comps.contains("context")) {
        ss << "**Context:** " << comps["context"].get<std::string>() << "\n\n";
      }
      if (comps.contains("constraints") && comps["constraints"].is_array()) {
        ss << "**Constraints:**\n";
        for (const auto &c : comps["constraints"]) {
          ss << "- " << c.get<std::string>() << "\n";
        }
        ss << "\n";
      }
      ss << "</details>";
    }

    // Fallback for previous schema or other keys if new schema missing
    if (!j.contains("meta_analysis") && !j.contains("optimized_prompt")) {
      ss << "## Result\n" << j.dump(2);
    }

    return ss.str();

  } catch (const std::exception &e) {
    std::cerr << "JSON Parsing failed: " << e.what()
              << "\nOriginal: " << jsonStr << std::endl;
    return jsonStr; // Fallback to raw if parsing fails
  }
}
