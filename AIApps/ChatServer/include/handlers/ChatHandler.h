// ChatHandler.h：GET /chat：验证登录，读取 AI.html 并向 head 注入用户 ID，返回聊天界面。
// 此 Handler 只返回页面；真正发消息走另外的 POST 接口。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"

class ChatHandler : public http::router::RouterHandler
{
public:
    explicit ChatHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};
