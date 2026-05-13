#ifndef AILIFE_LLM_CLIENT_H
#define AILIFE_LLM_CLIENT_H

#include <chrono>
#include <string>
#include <vector>

struct ToolParam {
    std::string name;
    std::string description;
};

struct ToolDef {
    std::string name;
    std::string description;
    std::vector<ToolParam> params;
};

struct ToolCall {
    std::string name;
    std::string arguments_json;
};

struct LLMRequest {
    std::string system_prompt;
    std::string user_prompt;
    int max_tokens{200};
    float temperature{0.8F};
    std::vector<ToolDef> tools;
};

struct LLMResponse {
    std::string content;
    std::vector<ToolCall> tool_calls;
};

class LLMClient {
  public:
    virtual ~LLMClient() = default;
    virtual LLMResponse complete(const LLMRequest& request) = 0;
};

class RealLLMClient : public LLMClient {
  public:
    RealLLMClient(std::string api_key, std::vector<std::string> models);
    LLMResponse complete(const LLMRequest& request) override;

  private:
    std::string api_key_;
    std::vector<std::string> models_;
    std::chrono::steady_clock::time_point last_call_{};
    std::string debug_path_;
};

class MockLLMClient : public LLMClient {
  public:
    LLMResponse complete(const LLMRequest& request) override;

  private:
    int calls_{0};
};

#endif // AILIFE_LLM_CLIENT_H
