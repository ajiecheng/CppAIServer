// ChatLogoutHandler.h：POST /user/logout：从 Session 取 userId，清空并删除会话，再从在线表移除用户。
// 不清除浏览器 Cookie，也不清空聊天历史；请求正文解析失败时前面的会话修改已发生。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"

class ChatLogoutHandler : public http::router::RouterHandler
{
public:
    explicit ChatLogoutHandler(ChatServer* server) : server_(server) {}
    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
};
