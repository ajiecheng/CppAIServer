// ChatRegisterHandler.cpp：POST /register：解析用户名密码，先检查存在性，再插入 users 并查询用户 ID。
// SQL 使用字符串拼接、密码直接存储，且检查/插入没有事务；异常交由上层处理。

#include "../include/handlers/ChatRegisterHandler.h"

// 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
void ChatRegisterHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // HTTP 层只提供正文字符串；这里才按 JSON 结构提取业务参数。
    json parsed = json::parse(req.getBody());
    std::string username = parsed["username"];
    std::string password = parsed["password"];

    int userId = insertUser(username, password);
    if (userId != -1)
    {
        json successResp;
        successResp["status"] = "success";
        successResp["message"] = "Register successful";
        successResp["userId"] = userId;
        // 业务 JSON 编码为正文；完整 HTTP 报文稍后由 HttpResponse::appendToBuffer 编码。
        std::string successBody = successResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
    }
    else
    {

        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = "username already exists";
        std::string failureBody = failureResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k409Conflict, "Conflict");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}

// 先查重，再拼接 SQL 插入并查询 ID；不是同一连接事务，插入异常也不等价于用户名已存在。
int ChatRegisterHandler::insertUser(const std::string& username, const std::string& password)
{
    if (!isUserExist(username))
    {
        std::string sql = "INSERT INTO users (username, password) VALUES ('"
            + username + "', '" + password + "')";
        mysqlUtil_.executeUpdate(sql);

        std::string sql2 = "SELECT id FROM users WHERE username = '" + username + "'";
        auto res = mysqlUtil_.executeQuery(sql2);
        if (res->next())
        {
            return res->getInt("id");
        }
    }
    return -1;
}

// 按用户名查 users 是否有记录；直接拼接未转义输入，有 SQL 注入风险。
bool ChatRegisterHandler::isUserExist(const std::string& username)
{
    std::string sql = "SELECT id FROM users WHERE username = '" + username + "'";
    auto res = mysqlUtil_.executeQuery(sql);
    if (res->next())
    {
        return true;
    }
    return false;
}
