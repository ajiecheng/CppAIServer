// AIToolRegistry.h：工具执行表：把工具名映射到 json → json 函数，构造时注册天气和本地时间。
// 天气通过同步 curl 访问 wttr.in，失败以错误 JSON 返回；未知工具名称抛异常。
// 工具描述来自 AIConfig，两者不会自动互相注册；名称及参数需要调用方保持一致。

#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <ctime>
#include <curl/curl.h>
#include "../../../../HttpServer/include/utils/JsonUtil.h"

class AIToolRegistry {
public:

    using ToolFunc = std::function<json(const json&)>;

    // 每个注册表实例注册 get_weather 和 get_time；配置里列出工具不会自动增加可执行函数。
    AIToolRegistry();

    // 以名称保存或覆盖函数对象，没有并发修改保护。
    void registerTool(const std::string& name, ToolFunc func);

    // 查找后同步执行工具，未知名称抛异常；工具返回错误 JSON 时这里仍正常返回。
    json invoke(const std::string& name, const json& args) const;

    // 只检查名称是否注册，不验证参数和外部服务可用性。
    bool hasTool(const std::string& name) const;

private:
    std::unordered_map<std::string, ToolFunc> tools_;

    // 把 curl 返回片段追加到 output 并返回字节数。
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    // 要求 city，URL 编码后同步请求 wttr.in，设置 5 秒超时；缺参或 curl 失败返回错误 JSON。
    static json getWeather(const json& args);
    // 使用进程本地时区生成当前时间字符串，忽略 args；localtime 共享静态状态需注意并发调用。
    static json getTime(const json& args);
};
