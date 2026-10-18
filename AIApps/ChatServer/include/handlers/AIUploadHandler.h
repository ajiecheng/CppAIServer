// AIUploadHandler.h：GET /upload：检查登录，读取上传页面并注入 userId。
// 路径相对工作目录；未登录返回 401，捕获到的标准异常转为 400。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class AIUploadHandler : public http::router::RouterHandler
{
public:
    explicit AIUploadHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
};
