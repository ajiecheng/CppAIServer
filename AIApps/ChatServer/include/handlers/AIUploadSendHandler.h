// AIUploadSendHandler.h：POST /upload/send：校验登录，读取 JSON 中的 filename/image，Base64 解码后本地分类。
// 按用户复用 ImageRecognizer；映射锁只保护查找/创建，不包住推理；confidence 固定 0.95，不是模型概率。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class AIUploadSendHandler : public http::router::RouterHandler
{
public:
    explicit AIUploadSendHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
};
