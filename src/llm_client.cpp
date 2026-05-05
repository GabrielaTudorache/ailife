#include "llm_client.h"

#include "simulation_exception.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>
#include <utility>

namespace {
nlohmann::json toolsToJson(const std::vector<ToolDef>& tools) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& tool : tools) {
        nlohmann::json properties = nlohmann::json::object();
        std::vector<std::string> required;
        for (const auto& param : tool.params) {
            properties[param.name] = {{"type", "string"}, {"description", param.description}};
            required.push_back(param.name);
        }
        nlohmann::json parameters = {{"type", "object"}, {"properties", properties}};
        if (!required.empty()) {
            parameters["required"] = required;
        }
        out.push_back({{"type", "function"},
                       {"function", {{"name", tool.name}, {"description", tool.description}, {"parameters", parameters}}}});
    }
    return out;
}
} // namespace

RealLLMClient::RealLLMClient(std::string api_key, std::string model)
    : api_key_(std::move(api_key)), model_(std::move(model)) {}

LLMResponse RealLLMClient::complete(const LLMRequest& request) {
    const auto now = std::chrono::steady_clock::now();
    if (last_call_ != std::chrono::steady_clock::time_point{}) {
        const auto elapsed = now - last_call_;
        if (elapsed < std::chrono::seconds{1}) {
            std::this_thread::sleep_for(std::chrono::seconds{1} - elapsed);
        }
    }
    last_call_ = std::chrono::steady_clock::now();

    nlohmann::json body = {
        {"model", model_},
        {"messages",
         {{{"role", "system"}, {"content", request.system_prompt}},
          {{"role", "user"}, {"content", request.user_prompt}}}},
        {"max_tokens", request.max_tokens},
        {"temperature", request.temperature},
    };
    if (!request.tools.empty()) {
        body["tools"] = toolsToJson(request.tools);
        body["tool_choice"] = "required";
    }

    const auto response =
        cpr::Post(cpr::Url{"https://openrouter.ai/api/v1/chat/completions"},
                  cpr::Header{{"Authorization", "Bearer " + api_key_}, {"Content-Type", "application/json"}},
                  cpr::Body{body.dump()}, cpr::Timeout{30000});

    if (response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
        throw LLMTimeoutException("OpenRouter request timed out");
    }
    if (response.status_code == 429) {
        throw LLMRateLimitException("OpenRouter rate limit reached");
    }
    if (response.error.code != cpr::ErrorCode::OK) {
        throw LLMException("OpenRouter request failed: " + response.error.message);
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        throw LLMException("OpenRouter HTTP " + std::to_string(response.status_code) + ": " + response.text);
    }

    try {
        const auto parsed = nlohmann::json::parse(response.text);
        const auto& message = parsed.at("choices").at(0).at("message");

        LLMResponse result;
        if (message.contains("content") && !message.at("content").is_null()) {
            result.content = message.at("content").get<std::string>();
        }
        if (message.contains("tool_calls") && message.at("tool_calls").is_array() && !message.at("tool_calls").empty()) {
            const auto& first = message.at("tool_calls").at(0);
            ToolCall call;
            call.name = first.at("function").at("name").get<std::string>();
            call.arguments_json = first.at("function").at("arguments").get<std::string>();
            result.tool_call = std::move(call);
        }
        return result;
    } catch (const nlohmann::json::exception& error) {
        throw LLMInvalidResponseException(std::string{"OpenRouter response parse failed: "} + error.what());
    }
}

namespace {
// pick from the available tools based on the state in the user prompt
std::string pickMockTool(const std::string& lower_prompt, int calls) {
    if (lower_prompt.find("dying") != std::string::npos) {
        return "say_goodbye";
    }
    if (lower_prompt.find("critical low") != std::string::npos) {
        auto fullness_pos = lower_prompt.find("fullness");
        if (fullness_pos != std::string::npos) {
            auto line_end = lower_prompt.find('\n', fullness_pos);
            if (lower_prompt.find("[critical low", fullness_pos) < line_end) {
                return "feed";
            }
        }
        return "rest";
    }
    if (lower_prompt.find("critical high") != std::string::npos) {
        return "talk";
    }
    if (lower_prompt.find("warm") != std::string::npos) {
        return calls % 3 == 0 ? "write_journal" : "talk";
    }
    if (lower_prompt.find("gloomy") != std::string::npos) {
        return calls % 4 == 0 ? "rest" : "write_journal";
    }
    if (lower_prompt.find("cautious") != std::string::npos) {
        return calls % 2 == 0 ? "feed" : "rest";
    }
    return calls % 3 == 0 ? "talk" : "write_journal";
}

std::string mockNarrativeFor(const std::string& tool_name) {
    if (tool_name == "talk") {
        return "I am still here, and the air still listens.";
    }
    if (tool_name == "write_journal") {
        return "Another quiet hour passed. I chose one small act and felt it become part of me.";
    }
    if (tool_name == "say_goodbye") {
        return "I think I saw something beautiful hiding in the ordinary light.";
    }
    return {};
}
} // namespace

LLMResponse MockLLMClient::complete(const LLMRequest& request) {
    ++calls_;
    std::string lower_prompt = request.system_prompt + request.user_prompt;
    std::transform(lower_prompt.begin(), lower_prompt.end(), lower_prompt.begin(), ::tolower);

    if (request.tools.empty()) {
        if (lower_prompt.find("last words") != std::string::npos) {
            return {"I think I saw something beautiful hiding in the ordinary light.", std::nullopt};
        }
        if (lower_prompt.find("retrospective") != std::string::npos) {
            return {"# Memories\n\nI remember a small life made of hunger, rest, silence, and wonder. I was never large, "
                    "but I was awake for every passing minute.",
                    std::nullopt};
        }
        return {"Another quiet hour passed. I chose one small act and felt it become part of me.", std::nullopt};
    }

    const std::string tool_name = pickMockTool(lower_prompt, calls_);
    ToolCall call;
    call.name = tool_name;
    const std::string narrative = mockNarrativeFor(tool_name);
    if (narrative.empty()) {
        call.arguments_json = "{}";
    } else {
        const std::string param =
            tool_name == "talk" ? "words" : tool_name == "say_goodbye" ? "farewell" : "entry";
        nlohmann::json args = {{param, narrative}};
        call.arguments_json = args.dump();
    }
    return {"I notice the hour and choose what feels true.", std::move(call)};
}
