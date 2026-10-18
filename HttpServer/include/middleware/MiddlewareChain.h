// MiddlewareChain.h：中间件执行器：before 按注册顺序，after 按逆序执行，形成前后对称的处理结构。
// before 异常向 HttpServer 传播；after 捕获 std::exception 后终止本轮后续执行。
// 这里没有线程切换或任务队列，调用发生在当前请求处理线程。

#pragma once

#include <vector>
#include <memory>
#include "Middleware.h"

namespace http
{
namespace middleware
{

class MiddlewareChain
{
public:

    // 追加共享中间件对象；运行中并发注册没有同步。
    void addMiddleware(std::shared_ptr<Middleware> middleware);

    // 正向执行 before，异常立即传播，不自动执行已进入中间件的 after。
    void processBefore(HttpRequest& request);

    // 逆序执行 after；一个标准异常会终止剩余 after，并记录日志。
    void processAfter(HttpResponse& response);

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};

}
}
