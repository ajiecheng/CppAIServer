// ChatSendHandler.h：POST /chat/send：验证登录，按用户/聊天会话取得 AIHelper，同步请求模型后返回 JSON。
// 查找锁释放后调用 chat；共享指针保证对象存活，不保证同一会话并发调用安全。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"

class ChatSendHandler : public http::router::RouterHandler
{
public:
    explicit ChatSendHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};
