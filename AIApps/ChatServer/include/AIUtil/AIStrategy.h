// AIStrategy.h：模型差异接口：端点、密钥、模型名、请求 JSON 和响应文本提取。
// Aliyun/DouBao 使用兼容聊天格式，RAG 使用应用端点；MCP 标志让 AIHelper 走提示词工具编排。
// 消息角色按下标奇偶推断；策略不负责网络执行，实际同步 curl 在 AIHelper 中。

#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>
#include <fstream>

#include "../../../../HttpServer/include/utils/JsonUtil.h"

// 优先读取指定环境变量；否则读取 HOME 下配置第一行，HOME 缺失才改读资源路径，均失败则抛异常。
inline std::string readApiKey(const char* envName) {
    const char* key = std::getenv(envName);
    if (key && strlen(key) > 0) {
        return std::string(key);
    }

    const char* home = std::getenv("HOME");
    std::string confPath = home
        ? (std::string(home) + "/.cppaiservice/apikey.conf")
        : "AIApps/ChatServer/resource/apikey.conf";
    std::ifstream conf(confPath);
    if (conf.is_open()) {
        std::string line;
        if (std::getline(conf, line) && !line.empty()) {
            return line;
        }
    }
    throw std::runtime_error(std::string(envName) + " not found in env or config file!");
}

class AIStrategy {
public:
    virtual ~AIStrategy() = default;

    // 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
    virtual std::string getApiUrl() const = 0;
    // 返回构造时已加载的密钥，不在此重新读取配置。
    virtual std::string getApiKey() const = 0;
    // 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
    virtual std::string getModel() const = 0;
    // 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
    virtual json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const = 0;
    // 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
    virtual std::string parseResponse(const json& response) const = 0;

    bool isMCPModel = false;
};

class AliyunStrategy : public AIStrategy {
public:
    AliyunStrategy() {
        apiKey_ = readApiKey("DASHSCOPE_API_KEY");
        isMCPModel = false;
    }
    // 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
    std::string getApiUrl() const override;
    // 返回构造时已加载的密钥，不在此重新读取配置。
    std::string getApiKey() const override;
    // 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
    std::string getModel() const override;
    // 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    // 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
    std::string parseResponse(const json& response) const override;
private:
    std::string apiKey_;
};

class DouBaoStrategy : public AIStrategy {
public:
    DouBaoStrategy() {
        apiKey_ = readApiKey("DOUBAO_API_KEY");
        isMCPModel = false;
    }
    // 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
    std::string getApiUrl() const override;
    // 返回构造时已加载的密钥，不在此重新读取配置。
    std::string getApiKey() const override;
    // 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
    std::string getModel() const override;
    // 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    // 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
    std::string parseResponse(const json& response) const override;
private:
    std::string apiKey_;
};

class AliyunRAGStrategy : public AIStrategy {
public:
    AliyunRAGStrategy() {
        apiKey_ = readApiKey("DASHSCOPE_API_KEY");
        isMCPModel = false;
    }
    // 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
    std::string getApiUrl() const override;
    // 返回构造时已加载的密钥，不在此重新读取配置。
    std::string getApiKey() const override;
    // 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
    std::string getModel() const override;
    // 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    // 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
    std::string parseResponse(const json& response) const override;
private:
    std::string apiKey_;
};

class AliyunMcpStrategy : public AIStrategy {
public:
    AliyunMcpStrategy() {
        apiKey_ = readApiKey("DASHSCOPE_API_KEY");
        isMCPModel = true;
    }
    // 返回该策略端点；RAG 另外要求 Knowledge_Base_ID 环境变量。
    std::string getApiUrl() const override;
    // 返回构造时已加载的密钥，不在此重新读取配置。
    std::string getApiKey() const override;
    // 返回当前策略模型名；RAG 应用端点不使用 model，返回空串。
    std::string getModel() const override;
    // 按该提供方格式组装历史；文本角色按下标奇偶推断，不使用 pair 中的时间戳。
    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    // 从该提供方预期结构提取回答；缺外层字段时返回空串，内层类型错误仍可能抛异常。
    std::string parseResponse(const json& response) const override;
private:
    std::string apiKey_;
};
