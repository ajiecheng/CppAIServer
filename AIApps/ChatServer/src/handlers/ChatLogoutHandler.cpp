// ChatLogoutHandler.cpp：POST /user/logout：从 Session 取 userId，清空并删除会话，再从在线表移除用户。
// 不清除浏览器 Cookie，也不清空聊天历史；请求正文解析失败时前面的会话修改已发生。

#include "../include/handlers/ChatLogoutHandler.h"

// 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
void ChatLogoutHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{

    auto contentType = req.getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(0);
        resp->setBody("");
        return;
    }

    try
    {
        // 从 Cookie 查找或创建服务端会话；获取会话本身不代表已通过登录校验。
        auto session = server_->getSessionManager()->getSession(req, resp);
        int userId = std::stoi(session->getValue("userId"));

        session->clear();

        server_->getSessionManager()->destroySession(session->getId());

        // HTTP 层只提供正文字符串；这里才按 JSON 结构提取业务参数。
        json parsed = json::parse(req.getBody());

        {
            std::lock_guard<std::mutex> lock(server_->mutexForOnlineUsers_);
            server_->onlineUsers_.erase(userId);
        }

        json response;
        response["message"] = "logout successful";
        std::string responseBody = response.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(responseBody.size());
        resp->setBody(responseBody);
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
    }
}
