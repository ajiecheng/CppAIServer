// ChatHistoryHandler.h：POST /chat/history：按登录用户和请求 sessionId 返回内存消息副本，不直接查询数据库。
// 不存在时创建空 AIHelper；is_user 按下标奇偶生成，不是读取恢复时的角色字段。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class ChatHistoryHandler : public http::router::RouterHandler
{
public:
    explicit ChatHistoryHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
};
