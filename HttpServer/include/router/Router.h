// Router.h：请求分发：以请求方法和路径查找对象处理器或函数回调。
// 匹配优先级：精准 Handler → 精准 Callback → 正则 Handler → 正则 Callback；未命中返回 false。
// 动态路径 /:name 转成捕获组，但提取键为 param1、param2，不保留 name；静态部分未转义正则元字符。
// 路由通常在启动阶段注册；容器无锁，不支持无同步的运行时并发增删。

#pragma once
#include <iostream>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <regex>
#include <vector>

#include "RouterHandler.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace router
{

class Router
{
public:
    using HandlerPtr = std::shared_ptr<RouterHandler>;
    using HandlerCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

    // 精准路由以方法和路径共同构成键，同路径不同方法可分别注册。
    struct RouteKey
    {
        HttpRequest::Method method;
        std::string path;

        bool operator==(const RouteKey &other) const
        {
            return method == other.method && path == other.path;
        }
    };

    // 精准路由以方法和路径共同构成键，同路径不同方法可分别注册。
    struct RouteKeyHash
    {
        size_t operator()(const RouteKey &key) const
        {
            size_t methodHash = std::hash<int>{}(static_cast<int>(key.method));
            size_t pathHash = std::hash<std::string>{}(key.path);
            return methodHash * 31 + pathHash;
        }
    };

    // 按方法和精确路径保存 shared_ptr；同键注册覆盖旧处理器。
    void registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler);

    // 按方法和精确路径保存函数对象；与 Handler 存在不同容器，匹配时 Handler 优先。
    void registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback);

    void addRegexHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
    {
        std::regex pathRegex = convertToRegex(path);
        regexHandlers_.emplace_back(method, pathRegex, handler);
    }

    void addRegexCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
    {
        std::regex pathRegex = convertToRegex(path);
        regexCallbacks_.emplace_back(method, pathRegex, callback);
    }

    // 命中后同步调用处理器并返回 true；false 只代表未找到路由，业务是否成功由响应表达。
    bool route(const HttpRequest &req, HttpResponse *resp);

private:

    // 把 /:参数 转成非斜杠捕获组，首尾锚定；没有转义其他正则元字符。
    std::regex convertToRegex(const std::string &pathPattern)
    {
        std::string regexPattern = "^"
            + std::regex_replace(pathPattern, std::regex(R"(/:([^/]+))"), R"(/([^/]+))")
            + "$";
        return std::regex(regexPattern);
    }

    // 跳过完整匹配 match[0]，把捕获组依次保存为 param1、param2 等。
    void extractPathParameters(const std::smatch &match, HttpRequest &request)
    {
        for (size_t i = 1; i < match.size(); ++i)
        {
            request.setPathParameters("param" + std::to_string(i), match[i].str());
        }
    }

private:

    struct RouteCallbackObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerCallback callback_;
        RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback &callback)
            : method_(method), pathRegex_(pathRegex), callback_(callback) {}
    };

    struct RouteHandlerObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerPtr handler_;
        RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex, HandlerPtr handler)
            : method_(method), pathRegex_(pathRegex), handler_(handler) {}
    };

    // 精准匹配表：平均常数时间查找；实际冲突与最坏复杂度由哈希容器决定。
    std::unordered_map<RouteKey, HandlerPtr, RouteKeyHash>   handlers_;
    std::unordered_map<RouteKey, HandlerCallback, RouteKeyHash> callbacks_;

    // 动态匹配表：按注册顺序逐条尝试，首个命中即返回。
    std::vector<RouteHandlerObj>  regexHandlers_;
    std::vector<RouteCallbackObj> regexCallbacks_;
};

}
}
