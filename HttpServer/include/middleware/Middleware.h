// Middleware.h：请求前后共用逻辑的接口；before 可修改请求，after 可修改响应。
// 实际遍历由 MiddlewareChain 的 vector 完成；nextMiddleware_ 当前未接入执行流程。

#pragma once

#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace middleware
{

class Middleware
{
public:
    virtual ~Middleware() = default;

    virtual void before(HttpRequest& request) = 0;

    virtual void after(HttpResponse& response) = 0;

    void setNext(std::shared_ptr<Middleware> next)
    {
        nextMiddleware_ = next;
    }

protected:
    std::shared_ptr<Middleware> nextMiddleware_;
};

}
}
