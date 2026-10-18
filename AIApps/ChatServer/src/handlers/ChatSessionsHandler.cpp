// ChatSessionsHandler.cpp：GET /chat/sessions：验证登录，复制该用户的内存会话 ID 列表，转换为 JSON。
// 列表读取用 mutexForSessionsId，新增路径却使用其他锁，不能宣称读写已完整同步。

#include "../include/handlers/ChatSessionsHandler.h"

// 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
void ChatSessionsHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
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

        std::vector<std::string> sessions;
        {
            std::lock_guard<std::mutex> lock(server_->mutexForSessionsId);
            sessions = server_->sessionsIdsMap[userId];
        }

        json successResp;
        successResp["success"] = true;

        json sessionArray = json::array();
        for (auto sid : sessions) {
            json s;
            s["sessionId"] = sid;
            s["name"] = "会话 " + sid;
            sessionArray.push_back(s);
        }
        successResp["sessions"] = sessionArray;

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
