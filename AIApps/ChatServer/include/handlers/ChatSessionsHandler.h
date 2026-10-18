// ChatSessionsHandler.h：GET /chat/sessions：验证登录，复制该用户的内存会话 ID 列表，转换为 JSON。
// 列表读取用 mutexForSessionsId，新增路径却使用其他锁，不能宣称读写已完整同步。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"

class ChatSessionsHandler : public http::router::RouterHandler
{
public:
    explicit ChatSessionsHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};
