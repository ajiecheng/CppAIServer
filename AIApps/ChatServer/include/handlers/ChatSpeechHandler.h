// ChatSpeechHandler.h：POST /chat/tts：验证登录，读 text 和百度凭据，创建合成器并返回音频 URL。
// token 获取和轮询均在请求线程同步执行；空 URL 当前仍被包装为 success=true。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class ChatSpeechHandler : public http::router::RouterHandler
{
public:
    explicit ChatSpeechHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
};
