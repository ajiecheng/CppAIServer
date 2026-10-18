// AIStrategy.cpp：模型差异接口：端点、密钥、模型名、请求 JSON 和响应文本提取。
// Aliyun/DouBao 使用兼容聊天格式，RAG 使用应用端点；MCP 标志让 AIHelper 走提示词工具编排。
// 消息角色按下标奇偶推断；策略不负责网络执行，实际同步 curl 在 AIHelper 中。

#include"../include/AIUtil/AIStrategy.h"
#include"../include/AIUtil/AIFactory.h"

// 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
std::string AliyunStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

// 返回构造时已加载的密钥，不在此重新读取配置。
std::string AliyunStrategy::getApiKey() const {
    return apiKey_;
}

// 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
std::string AliyunStrategy::getModel() const {
    return "qwen-plus";
}

// 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
json AliyunStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;

        msg["role"] = (i % 2 == 0) ? "user" : "assistant";
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}

// 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
std::string AliyunStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}

// 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
std::string DouBaoStrategy::getApiUrl() const {
    return "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}

// 返回构造时已加载的密钥，不在此重新读取配置。
std::string DouBaoStrategy::getApiKey() const {
    return apiKey_;
}

// 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
std::string DouBaoStrategy::getModel() const {
    return "doubao-seed-1-6-thinking-250715";
}

// 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
json DouBaoStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        msg["role"] = (i % 2 == 0) ? "user" : "assistant";
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}

// 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
std::string DouBaoStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}

// 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
std::string AliyunRAGStrategy::getApiUrl() const {
    const char* key = std::getenv("Knowledge_Base_ID");
    if (!key) throw std::runtime_error("Knowledge_Base_ID not found!");
    std::string id(key);

    return "https://dashscope.aliyuncs.com/api/v1/apps/" + id + "/completion";
}

// 返回构造时已加载的密钥，不在此重新读取配置。
std::string AliyunRAGStrategy::getApiKey() const {
    return apiKey_;
}

// 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
std::string AliyunRAGStrategy::getModel() const {
    return "";
}

// 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
json AliyunRAGStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    json msgArray = json::array();
    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        msg["role"] = (i % 2 == 0 ? "user" : "assistant");
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["input"]["messages"] = msgArray;
    payload["parameters"] = json::object();
    return payload;
}

// 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
std::string AliyunRAGStrategy::parseResponse(const json& response) const {
    if (response.contains("output") && response["output"].contains("text")) {
        return response["output"]["text"];
    }
    return {};
}

// 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
std::string AliyunMcpStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

// 返回构造时已加载的密钥，不在此重新读取配置。
std::string AliyunMcpStrategy::getApiKey() const {
    return apiKey_;
}

// 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
std::string AliyunMcpStrategy::getModel() const {
    return "qwen-plus";
}

// 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
json AliyunMcpStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        msg["role"] = (i % 2 == 0) ? "user" : "assistant";
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}

// 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
std::string AliyunMcpStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}

static StrategyRegister<AliyunStrategy>    regAliyun("1");
static StrategyRegister<DouBaoStrategy>    regDoubao("2");
static StrategyRegister<AliyunRAGStrategy> regAliyunRag("3");
static StrategyRegister<AliyunMcpStrategy> regAliyunMcp("4");
