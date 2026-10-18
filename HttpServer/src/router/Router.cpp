// Router.cpp：请求分发：以请求方法和路径查找对象处理器或函数回调。
// 匹配优先级：精准 Handler → 精准 Callback → 正则 Handler → 正则 Callback；未命中返回 false。
// 动态路径 /:name 转成捕获组，但提取键为 param1、param2，不保留 name；静态部分未转义正则元字符。
// 路由通常在启动阶段注册；容器无锁，不支持无同步的运行时并发增删。

#include "../../include/router/Router.h"
#include <muduo/base/Logging.h>

namespace http
{
namespace router
{

// 按方法和精确路径保存 shared_ptr；同键注册覆盖旧处理器。
void Router::registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
{
    RouteKey key{method, path};
    handlers_[key] = std::move(handler);
}

// 按方法和精确路径保存函数对象；与 Handler 存在不同容器，匹配时 Handler 优先。
void Router::registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
{
    RouteKey key{method, path};
    callbacks_[key] = std::move(callback);
}

// 命中后同步调用处理器并返回 true；false 只代表未找到路由，业务是否成功由响应表达。
bool Router::route(const HttpRequest &req, HttpResponse *resp)
{
    RouteKey key{req.method(), req.path()};

    // 优先级 1：精准 Handler。
    auto handlerIt = handlers_.find(key);
    if (handlerIt != handlers_.end())
    {
        handlerIt->second->handle(req, resp);
        return true;
    }

    // 优先级 2：精准函数回调。
    auto callbackIt = callbacks_.find(key);
    if (callbackIt != callbacks_.end())
    {
        callbackIt->second(req, resp);
        return true;
    }

    // 优先级 3：动态 Handler，向 newReq 写入路径参数后传给处理器。
    for (const auto &[method, pathRegex, handler] : regexHandlers_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        if (method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {

            HttpRequest newReq(req);
            extractPathParameters(match, newReq);
            handler->handle(newReq, resp);
            return true;
        }
    }

    // 优先级 4：动态函数回调。
    for (const auto &[method, pathRegex, callback] : regexCallbacks_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        if (method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
            HttpRequest newReq(req);
            extractPathParameters(match, newReq);
            // 当前实现缺口：参数写在 newReq，实际却传 req，回调无法收到刚提取的参数。
            callback(req, resp);
            return true;
        }
    }

    return false;
}

}
}
