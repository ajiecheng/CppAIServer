// ChatEntryHandler.h：GET / 或 /entry：读取登录/注册入口 HTML，设置响应状态、长度和正文。
// 无 Session 鉴权；回退页面内容不自动变成 HTTP 404。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class ChatEntryHandler : public http::router::RouterHandler
{
public:
    explicit ChatEntryHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;

};
