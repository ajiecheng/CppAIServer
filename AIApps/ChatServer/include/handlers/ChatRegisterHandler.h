// ChatRegisterHandler.h：POST /register：解析用户名密码，先检查存在性，再插入 users 并查询用户 ID。
// SQL 使用字符串拼接、密码直接存储，且检查/插入没有事务；异常交由上层处理。

#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"

class ChatRegisterHandler : public http::router::RouterHandler
{
public:
    explicit ChatRegisterHandler(ChatServer* server) : server_(server) {}

    // 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:
    // 先查重，再拼接 SQL 插入并查询 ID；不是同一连接事务，插入异常也不等价于用户名已存在。
    int insertUser(const std::string& username, const std::string& password);
    // 按用户名查 users 是否有记录；直接拼接未转义输入，有 SQL 注入风险。
    bool isUserExist(const std::string& username);
private:
    // 借用业务服务对象，不负责 delete；该对象必须覆盖处理器的使用期。
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};
