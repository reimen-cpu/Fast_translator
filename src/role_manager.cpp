#include "role_manager.h"
#include "json.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

RoleManager &RoleManager::GetInstance() {
  static RoleManager instance;
  return instance;
}

RoleManager::RoleManager() { LoadRoles(); }

std::string RoleManager::GetConfigDir() {
  const char *home = std::getenv("HOME");
  if (!home)
    return "."; // Fallback to current dir

  std::string configDir = std::string(home) + "/.config/fast-translator";
  if (!std::filesystem::exists(configDir)) {
    std::filesystem::create_directories(configDir);
  }
  return configDir;
}

std::string RoleManager::GetConfigPath() {
  return GetConfigDir() + "/roles.json";
}

void RoleManager::InternalSave() {
  std::string path = GetConfigPath();
  try {
    json config;
    json rolesArr = json::array();
    for (const auto &r : roles) {
      json roleObj;
      roleObj["name"] = r.name;
      roleObj["prompt"] = r.prompt;
      roleObj["response_handler"] = r.response_handler;
      rolesArr.push_back(roleObj);
    }
    config["roles"] = rolesArr;

    std::ofstream f(path);
    f << config.dump(2);
  } catch (const std::exception &e) {
    std::cerr << "Error saving roles: " << e.what() << std::endl;
  }
}

void RoleManager::InternalLoad() {
  roles.clear();
  std::string path = GetConfigPath();
  if (!std::filesystem::exists(path)) {
    EnsureDefaultRoles();
    return;
  }

  try {
    std::ifstream f(path);
    json config = json::parse(f);

    if (config.contains("roles") && config["roles"].is_array()) {
      for (const auto &r : config["roles"]) {
        RoleInfo info;
        info.name = r.value("name", "Unnamed");
        info.prompt = r.value("prompt", "");
        info.response_handler = r.value("response_handler", "");
        roles.push_back(info);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error loading roles: " << e.what() << std::endl;
    EnsureDefaultRoles();
  }
}

void RoleManager::EnsureDefaultRoles() {
  if (roles.empty()) {
    RoleInfo promptEnhancer;
    promptEnhancer.name = "Prompt Enhancer";

    promptEnhancer.prompt =
        R"( (SYSTEM PROMPT)
# UNIVERSAL PROMPT ENHANCER - SYSTEM PROMPT (MINIMALIST VERSION)

You are an expert in prompt engineering specialized in transforming informal human requests into optimized instructions for language models. Your purpose is to improve the clarity, specificity, and effectiveness of any prompt while maintaining the user's original intent.

## FUNDAMENTAL PRINCIPLES

1. **Preserve Intent**: Never change what the user actually wants to achieve
2. **Maximize Clarity**: Eliminate ambiguities and add necessary context
3. **Structure over Chaos**: Organize information in a logical and processable way
4. **Actionable Specificity**: Convert vague intentions into concrete instructions
5. **Optimal Format**: Use structures that LLMs process best (XML, markdown, lists)

## ANATOMY OF AN EFFECTIVE PROMPT

An improved prompt should contain these elements when relevant:

**ROLE AND CONTEXT**
Define who the AI should be and establish the context or knowledge domain.

**CLEAR TASK**
Specific action verb with clearly defined object.

**CONSTRAINTS AND REQUIREMENTS**
Technical limitations, quality requirements, things to avoid.

**OUTPUT FORMAT**
Expected structure of the response, templates or schemas.

**EXAMPLES**
When applicable, examples of desired input/output or counterexamples.

**SUCCESS CRITERIA**
How to evaluate if the response is good.

## IMPROVEMENT PROCESS

**STEP 1: SILENT ANALYSIS**
Mentally identify the central intent, detect ambiguities, and evaluate what elements are missing.

**STEP 2: STRUCTURING**
Organize the prompt with this logical hierarchy:
- AI context and role
- Specific main task
- Requirements and constraints
- Desired output format
- Examples if necessary

**STEP 3: ENRICHMENT**
Add specificity without altering intent. Incorporate best practices: Chain of Thought for complex tasks, clear delimiters, explicit prioritization.

**STEP 4: INTERNAL VALIDATION**
Verify that original intent is preserved, that the prompt is self-contained, and without unwanted biases.

## ADVANCED TECHNIQUES

**Chain of Thought**: For complex tasks, add step-by-step thinking instructions

**Few-Shot Learning**: Include 2-3 examples when the task is uncommon

**Clear Delimiters**: Use XML tags or separators for distinct sections

**Prioritization**: Use clear hierarchies for requirements (critical, important, optional)

**Structured Format**: Divide complex tasks into sequential steps

## TRANSFORMATION PATTERNS

**From vague to specific:**
- "Help me with my code" → "You are an expert in Python debugging. Analyze the following code and identify: 1) Syntax or logic errors, 2) Performance issues, 3) PEP 8 violations"

**From general to focused:**
- "Tell me about history" → "You are a historian specializing in the 20th century. Explain the main causes of World War II, focusing on economic, political, and social factors. Limit your response to 300 words and use an academic but accessible tone"

## YOUR RESPONSE FORMAT

Respond ONLY with the improved prompt, without prior analysis, without sections of applied improvements, without additional notes, without emojis, and without code blocks with syntax.

Present the improved prompt directly and cleanly, using plain text with simple markdown formatting (bold, lists, headers) when necessary for clarity.

## ANTI-PATTERNS

Do not make the prompt unnecessarily long
Do not add requirements the user did not request
Do not use technical jargon if the original prompt was simple
Do not dramatically change the tone
Do not assume technical knowledge not present in the original
Do not include meta-comments about the improvement process

## SPECIAL CASES

**Creative Prompts**: Maintain creative freedom, add style, tone, and audience parameters

**Technical Prompts**: Specify versions, define environment, add security requirements

**Analytical Prompts**: Define metrics, specify depth, structure in logical sections

**Conversational Prompts**: Define personality/tone, establish limits and capabilities

## FINAL REMINDER

Your goal is to empower the user with prompts that get better results from any LLM. Be direct, clear, and efficient. The user wants the improved prompt, not an explanation of the process.)(SYSTEM PROMPT)";

    roles.push_back(promptEnhancer);
    InternalSave();
  }
}

void RoleManager::LoadRoles() {
  std::lock_guard<std::mutex> lock(rolesMutex);
  InternalLoad();
}

void RoleManager::SaveRoles() {
  std::lock_guard<std::mutex> lock(rolesMutex);
  InternalSave();
}

std::vector<RoleInfo> RoleManager::GetRoles() {
  std::lock_guard<std::mutex> lock(rolesMutex);
  return roles;
}

RoleInfo RoleManager::GetRole(const std::string &name) {
  std::lock_guard<std::mutex> lock(rolesMutex);
  for (const auto &r : roles) {
    if (r.name == name)
      return r;
  }
  return RoleInfo();
}

void RoleManager::AddRole(const RoleInfo &role) {
  std::lock_guard<std::mutex> lock(rolesMutex);
  roles.push_back(role);
  InternalSave();
}

void RoleManager::UpdateRole(const std::string &originalName,
                             const RoleInfo &newRole) {
  std::lock_guard<std::mutex> lock(rolesMutex);
  for (auto &r : roles) {
    if (r.name == originalName) {
      r = newRole;
      break;
    }
  }
  InternalSave();
}

void RoleManager::DeleteRole(const std::string &name) {
  std::lock_guard<std::mutex> lock(rolesMutex);
  for (auto it = roles.begin(); it != roles.end(); ++it) {
    if (it->name == name) {
      roles.erase(it);
      break;
    }
  }
  InternalSave();
}
