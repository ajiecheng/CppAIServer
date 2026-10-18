// ChatLoginHandler.cpp：POST /login：校验 JSON 请求，参数化查询账号密码，验证通过后获取/创建登录 Session。
// 先写 Session 身份再检查重复登录，拒绝时未撤销身份；在线表读取在锁外，有并发缺口。

#include "../include/handlers/ChatLoginHandler.h"

// 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
void ChatLoginHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{

    auto contentType = req.getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        LOG_INFO << "content" << req.getBody();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(0);
        resp->setBody("");
        return;
    }

    try
    {

        // HTTP 层只提供正文字符串；这里才按 JSON 结构提取业务参数。
        json parsed = json::parse(req.getBody());
        std::string username = parsed["username"];
        std::string password = parsed["password"];

        int userId = queryUserId(username, password);
        if (userId != -1)
        {

            // 从 Cookie 查找或创建服务端会话；获取会话本身不代表已通过登录校验。
            auto session = server_->getSessionManager()->getSession(req, resp);

            session->setValue("userId", std::to_string(userId));
            session->setValue("username", username);
            // 这里早于重复登录检查；后面即使返回 403，该登录标记也未撤销。
            session->setValue("isLoggedIn", "true");

            if (server_->onlineUsers_.find(userId) == server_->onlineUsers_.end()
                || server_->onlineUsers_[userId] == false)
            {
                {
                    std::lock_guard<std::mutex> lock(server_->mutexForOnlineUsers_);
                    server_->onlineUsers_[userId] = true;
                }

                json successResp;
                successResp["success"] = true;
                successResp["userId"] = userId;
                // 业务 JSON 编码为正文；完整 HTTP 报文稍后由 HttpResponse::appendToBuffer 编码。
                std::string successBody = successResp.dump(4);

                resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
                resp->setCloseConnection(false);
                resp->setContentType("application/json");
                resp->setContentLength(successBody.size());
                resp->setBody(successBody);
                return;
            }
            else
            {

                json failureResp;
                failureResp["success"] = false;
                failureResp["error"] = "重复登录";
                std::string failureBody = failureResp.dump(4);

                resp->setStatusLine(req.getVersion(), http::HttpResponse::k403Forbidden, "Forbidden");
                resp->setCloseConnection(true);
                resp->setContentType("application/json");
                resp->setContentLength(failureBody.size());
                resp->setBody(failureBody);
                return;
            }
        }
        else
        {

            json failureResp;
            failureResp["status"] = "error";
            failureResp["message"] = "Invalid username or password";
            std::string failureBody = failureResp.dump(4);

            resp->setStatusLine(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(failureBody.size());
            resp->setBody(failureBody);
            return;
        }
    }
    catch (const std::exception& e)
    {
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
        return;
    }
}

// 用两个占位符绑定账号密码，查到返回用户 ID，未查到返回 -1；密码仍为明文比对。
int ChatLoginHandler::queryUserId(const std::string& username, const std::string& password)
{
    std::string sql = "SELECT id FROM users WHERE username = ? AND password = ?";
    auto res = mysqlUtil_.executeQuery(sql, username, password);
    if (res->next())
    {
        return res->getInt("id");
    }
    return -1;
}
