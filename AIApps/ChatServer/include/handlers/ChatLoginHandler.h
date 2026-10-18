// ChatLoginHandler.h：POST /login：校验 JSON 请求，参数化查询账号密码，验证通过后获取/创建登录 Session。
// 先写 Session 身份再检查重复登录，拒绝时未撤销身份；在线表读取在锁外，有并发缺口。

#pragma once

#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include"../ChatServer.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"

class ChatLoginHandler : public http::router::RouterHandler
{
public:
    explicit ChatLoginHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    // 用两个占位符绑定账号密码，查到返回用户 ID，未查到返回 -1；密码仍为明文比对。
    int queryUserId(const std::string& username, const std::string& password);

private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};
