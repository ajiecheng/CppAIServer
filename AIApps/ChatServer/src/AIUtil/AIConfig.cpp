// AIConfig.cpp：提示词工具调用配置：加载模板和工具描述，格式化提示词，并解析模型返回的工具名/参数。
// AITool 是描述数据，AIToolCall 是模型决策；真正执行工具由 AIToolRegistry 完成。
// 这是应用内提示词/JSON 约定，不包含标准 MCP 传输、握手或 JSON-RPC 会话实现。

#include"../include/AIUtil/AIConfig.h"

// 读取模板及工具描述；文件/模板检查失败返回 false，JSON 类型异常可传播，重复加载不清空 tools_。
bool AIConfig::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[AIConfig] Unable to open configuration file: " << path << std::endl;
        return false;
    }

    json j;
    file >> j;

    if (!j.contains("prompt_template") || !j["prompt_template"].is_string()) {
        std::cerr << "[AIConfig] prompt_template is missing" << std::endl;
        return false;
    }
    promptTemplate_ = j["prompt_template"].get<std::string>();

    if (j.contains("tools") && j["tools"].is_array()) {
        for (auto& tool : j["tools"]) {
            AITool t;
            t.name = tool.value("name", "");
            t.desc = tool.value("desc", "");
            if (tool.contains("params") && tool["params"].is_object()) {
                for (auto& [key, val] : tool["params"].items()) {
                    t.params[key] = val.get<std::string>();
                }
            }
            tools_.push_back(std::move(t));
        }
    }
    return true;
}

// 把工具名、参数名、描述格式化为文本；unordered_map 参数遍历顺序不固定。
std::string AIConfig::buildToolList() const {
    std::ostringstream oss;
    for (const auto& t : tools_) {
        oss << t.name << "(";
        bool first = true;
        for (const auto& [key, val] : t.params) {
            if (!first) oss << ", ";
            oss << key;
            first = false;
        }
        oss << ") → " << t.desc << "\n";
    }
    return oss.str();
}

// 用 regex_replace 注入用户问题和工具列表；替换串中的特殊替换语法仍可能被解释。
std::string AIConfig::buildPrompt(const std::string& userInput) const {
    std::string result = promptTemplate_;
    result = std::regex_replace(result, std::regex("\\{user_input\\}"), userInput);
    result = std::regex_replace(result, std::regex("\\{tool_list\\}"), buildToolList());
    return result;
}

// 仅把含字符串 tool 字段的 JSON 识别为工具调用；不验证工具存在或必需参数，解析失败按普通回答处理。
AIToolCall AIConfig::parseAIResponse(const std::string& response) const {
    AIToolCall result;
    try {
        json j = json::parse(response);

        if (j.contains("tool") && j["tool"].is_string()) {
            result.toolName = j["tool"].get<std::string>();
            if (j.contains("args") && j["args"].is_object()) {
                result.args = j["args"];
            }
            result.isToolCall = true;
        }

    }
    catch (...) {

        result.isToolCall = false;
    }
    return result;
}

// 把原问题、工具名、参数和结果组成第二轮提示词，不在此执行工具或网络请求。
std::string AIConfig::buildToolResultPrompt(
    const std::string& userInput,
    const std::string& toolName,
    const json& toolArgs,
    const json& toolResult) const
{
    std::ostringstream oss;
    oss << "下面是用户说的话：" << userInput << "\n"
        << "我刚才调用了工具 [" << toolName << "] ，参数为："
        << toolArgs.dump() << "\n"
        << "工具返回的结果如下：\n" << toolResult.dump(4) << "\n"
        << "请根据以上信息，用自然语言回答用户。";
    return oss.str();
}
