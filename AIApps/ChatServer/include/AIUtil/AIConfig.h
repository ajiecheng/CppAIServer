// AIConfig.h：提示词工具调用配置：加载模板和工具描述，格式化提示词，并解析模型返回的工具名/参数。
// AITool 是描述数据，AIToolCall 是模型决策；真正执行工具由 AIToolRegistry 完成。
// 这是应用内提示词/JSON 约定，不包含标准 MCP 传输、握手或 JSON-RPC 会话实现。

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
#include "../../../../HttpServer/include/utils/JsonUtil.h"

struct AITool {
    std::string name;
    std::unordered_map<std::string, std::string> params;
    std::string desc;
};

struct AIToolCall {
    std::string toolName;
    json args;
    bool isToolCall = false;
};

class AIConfig {
public:

    // 读取模板及工具描述；文件/模板检查失败返回 false，JSON 类型异常可传播，重复加载不清空 tools_。
    bool loadFromFile(const std::string& path);

    // 用 regex_replace 注入用户问题和工具列表；替换串中的特殊替换语法仍可能被解释。
    std::string buildPrompt(const std::string& userInput) const;

    // 仅把含字符串 tool 字段的 JSON 识别为工具调用；不验证工具存在或必需参数，解析失败按普通回答处理。
    AIToolCall parseAIResponse(const std::string& response) const;

    // 把原问题、工具名、参数和结果组成第二轮提示词，不在此执行工具或网络请求。
    std::string buildToolResultPrompt(const std::string& userInput,
                                      const std::string& toolName,
                                      const json& toolArgs,
                                      const json& toolResult) const;

private:

    // 把工具名、参数名、描述格式化为文本；unordered_map 参数遍历顺序不固定。
    std::string buildToolList() const;

    std::string promptTemplate_;
    std::vector<AITool> tools_;
};
