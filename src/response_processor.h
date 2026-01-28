#ifndef RESPONSE_PROCESSOR_H
#define RESPONSE_PROCESSOR_H

#include <string>

class ResponseProcessor {
public:
  static std::string Process(const std::string &rawResponse,
                             const std::string &handler);

private:
  static std::string JsonToMarkdown(const std::string &jsonStr);
};

#endif
