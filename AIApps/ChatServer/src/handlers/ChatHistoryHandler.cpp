// ChatHistoryHandler.cpp：POST /chat/history：按登录用户和请求 sessionId 返回内存消息副本，不直接查询数据库。
// 不存在时创建空 AIHelper；is_user 按下标奇偶生成，不是读取恢复时的角色字段。

#include "../include/handlers/ChatHistoryHandler.h"

// 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
void ChatHistoryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {

        // 从 Cookie 查找或创建服务端会话；获取会话本身不代表已通过登录校验。
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true")
        {
            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);
            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                "Unauthorized", true, "application/json", errorBody.size(), errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string sessionId;
        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("sessionId")) sessionId = j["sessionId"];
        }

        std::vector<std::pair<std::string, long long>> messages;
        {
            std::lock_guard<std::mutex> lock(server_->mutexForChatInformation);
            auto& userSessions = server_->chatInformation[userId];
            if (userSessions.find(sessionId) == userSessions.end()) {
                userSessions.emplace(sessionId, std::make_shared<AIHelper>());
            }
            messages = userSessions[sessionId]->GetMessages();
        }

        json successResp;
        successResp["success"] = true;
        successResp["history"] = json::array();

        for (size_t i = 0; i < messages.size(); ++i) {
            json msgJson;
            msgJson["is_user"] = (i % 2 == 0);
            msgJson["content"] = messages[i].first;
            successResp["history"].push_back(msgJson);
        }

        // 业务 JSON 编码为正文；完整 HTTP 报文稍后由 HttpResponse::appendToBuffer 编码。
        std::string successBody = successResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
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
