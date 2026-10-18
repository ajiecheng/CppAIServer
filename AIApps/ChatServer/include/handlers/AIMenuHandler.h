// AIMenuHandler.h：GET /menu：检查登录 Session，读取 menu.html，注入 userId 后返回页面。
// 文件不存在时改读回退页面，但仍按 200 响应；身份依据是服务端 Session。

#pragma once
#include "../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class AIMenuHandler : public http::router::RouterHandler
{
public:
    explicit AIMenuHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
};
