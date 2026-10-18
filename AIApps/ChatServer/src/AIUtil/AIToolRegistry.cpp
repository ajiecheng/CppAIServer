// AIToolRegistry.cpp：工具执行表：把工具名映射到 json → json 函数，构造时注册天气和本地时间。
// 天气通过同步 curl 访问 wttr.in，失败以错误 JSON 返回；未知工具名称抛异常。
// 工具描述来自 AIConfig，两者不会自动互相注册；名称及参数需要调用方保持一致。

#include "../include/AIUtil/AIToolRegistry.h"
#include <sstream>

// 每个注册表实例注册 get_weather 和 get_time；配置里列出工具不会自动增加可执行函数。
AIToolRegistry::AIToolRegistry() {
    registerTool("get_weather", getWeather);
    registerTool("get_time", getTime);
}

// 以名称保存或覆盖函数对象，没有并发修改保护。
void AIToolRegistry::registerTool(const std::string& name, ToolFunc func) {
    tools_[name] = func;
}

// 查找后同步执行工具，未知名称抛异常；工具返回错误 JSON 时这里仍正常返回。
json AIToolRegistry::invoke(const std::string& name, const json& args) const {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        throw std::runtime_error("Tool not found: " + name);
    }
    return it->second(args);
}

// 只检查名称是否注册，不验证参数和外部服务可用性。
bool AIToolRegistry::hasTool(const std::string& name) const {
    return tools_.count(name) > 0;
}

// 把 curl 返回片段追加到 output 并返回字节数。
size_t AIToolRegistry::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// 要求 city，URL 编码后同步请求 wttr.in，设置 5 秒超时；缺参或 curl 失败返回错误 JSON。
json AIToolRegistry::getWeather(const json& args) {
    if (!args.contains("city")) {
        return json{ {"error", "Missing parameter: city"} };
    }

    std::string city = args["city"].get<std::string>();
    std::string encodedCity;

    char* encoded = curl_easy_escape(nullptr, city.c_str(), city.length());
    if (encoded) {
        encodedCity = encoded;
        curl_free(encoded);
    } else {
        return json{ {"error", "URL encode failed"} };
    }

    std::string url = "https://wttr.in/" + encodedCity + "?format=3&lang=zh";

    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) {
        return json{ {"error", "Failed to init CURL"} };
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return json{ {"error", "CURL request failed"} };
    }

    return json{ {"city", city}, {"weather", response} };
}

// 使用进程本地时区生成当前时间字符串，忽略 args；localtime 共享静态状态需注意并发调用。
json AIToolRegistry::getTime(const json& args) {
    (void)args;
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now);
    return json{ {"time", buffer} };
}
