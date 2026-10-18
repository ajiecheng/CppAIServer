// ChatCreateAndSendHandler.h：POST /chat/send-new-session：验证登录，生成聊天会话 ID，创建 AIHelper 并发送首条问题。
// chatInformation 与会话列表在同一代码块修改，但读取列表使用另一把锁，当前同步不一致。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"

#include"../AIUtil/AISessionIdGenerator.h"
#include "../ChatServer.h"

class ChatCreateAndSendHandler : public http::router::RouterHandler
{
public:
    explicit ChatCreateAndSendHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};
