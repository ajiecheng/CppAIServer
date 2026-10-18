// ChatServer.cpp：业务装配层：拥有 HttpServer，初始化数据库、内存登录会话、CORS 和 14 条方法/路径映射。
// 请求链：HttpServer → Router → Handler；Handler 借用 ChatServer 访问业务共享状态。
// chatInformation 按用户 ID、聊天会话 ID 索引 AIHelper；这与 Cookie 登录 Session 是两套状态。
// 共享容器的锁不自动保护容器中 AIHelper 的内部数据；各调用点仍需核对同步范围。

#include "../include/handlers/ChatLoginHandler.h"
#include "../include/handlers/ChatRegisterHandler.h"
#include "../include/handlers/ChatLogoutHandler.h"
#include"../include/handlers/ChatHandler.h"
#include"../include/handlers/ChatEntryHandler.h"
#include"../include/handlers/ChatSendHandler.h"
#include"../include/handlers/AIMenuHandler.h"
#include"../include/handlers/AIUploadSendHandler.h"
#include"../include/handlers/AIUploadHandler.h"
#include"../include/handlers/ChatHistoryHandler.h"
#include"../include/handlers/ChatCreateAndSendHandler.h"
#include"../include/handlers/ChatSessionsHandler.h"
#include"../include/handlers/ChatSpeechHandler.h"

#include "../include/ChatServer.h"
#include "../../../HttpServer/include/http/HttpRequest.h"
#include "../../../HttpServer/include/http/HttpResponse.h"
#include "../../../HttpServer/include/http/HttpServer.h"

using namespace http;

// 先构造成员，再执行 initialize。注意：第 3 个实参 option 实际传给 HttpServer 的 bool useSSL，TCP option 未正确转发。
ChatServer::ChatServer(int port,const std::string& name,
 muduo::net::TcpServer::Option option)
    : httpServer_(port, name, option)
{
    initialize();
}

// 同步初始化数据库池 → 内存 Session → CORS → 路由；失败会影响后续装配，不是后台异步初始化。
void ChatServer::initialize() {
    std::cout << "聊天服务初始化开始 ! " << std::endl;

    // 预建 5 条数据库连接；地址/凭据当前硬编码，连接失败会中断构造。
    http::MysqlUtil::init("tcp://127.0.0.1:3306", "root", "123456", "ChatHttpServer", 5);

    initializeSession();

    initializeMiddleware();

    initializeRouter();
}

// 启动阶段读取历史，仅恢复内存，不重新发布写库消息。
void ChatServer::initChatMessage() {
    std::cout << "开始恢复对话" << std::endl;
    readDataFromMySQL();
    std::cout << "成功恢复对话"<< std::endl;
}

// 按时间和 id 遍历 chat_message，依 id/session_id 创建或复用 AIHelper；读取了 is_user，但 restoreMessage 未保存角色。
void ChatServer::readDataFromMySQL() {
    std::string sql = "SELECT id, username,session_id, is_user, content, ts FROM chat_message ORDER BY ts ASC, id ASC";

    QueryResult res;
    try {
        res = mysqlUtil_.executeQuery(sql);
    }
    catch (const std::exception& e) {
        std::cerr << "MySQL query failed: " << e.what() << std::endl;
        return;
    }

    while (res->next()) {
        long long user_id = 0;
        std::string session_id;
        std::string username, content;
        long long ts = 0;
        int is_user = 1;

        try {
            user_id    = res->getInt64("id");
            session_id = res->getString("session_id");
            username   = res->getString("username");
            content    = res->getString("content");
            ts         = res->getInt64("ts");
            is_user    = res->getInt("is_user");
        }
        catch (const std::exception& e) {
            std::cerr << "无法读取行: " << e.what() << std::endl;
            continue;
        }

        auto& userSessions = chatInformation[user_id];
        std::shared_ptr<AIHelper> helper;
        auto itSession = userSessions.find(session_id);
        if (itSession == userSessions.end()) {

            helper = std::make_shared<AIHelper>();
            userSessions[session_id] = helper;
            sessionsIdsMap[user_id].push_back(session_id);
        } else {

            helper = itSession->second;
        }

        // 只恢复文本和时间，刚读取的 is_user 未传入，角色信息丢失。
        helper->restoreMessage(content, ts);
    }

    std::cout << "readDataFromMySQL finished" << std::endl;
}

// 向 HTTP/muduo 层转发线程数配置，这一句本身不负责启动工作线程。
void ChatServer::setThreadNum(int numThreads) {
    httpServer_.setThreadNum(numThreads);
}

// 转交 HTTP 层启动；正常运行期间主线程停留在其事件循环中。
void ChatServer::start() {
    httpServer_.start();
}

// 在启动阶段注册方法和精确路径，共 14 条映射；业务当前未注册动态路径。
void ChatServer::initializeRouter() {

    httpServer_.Get("/", std::make_shared<ChatEntryHandler>(this));
    httpServer_.Get("/entry", std::make_shared<ChatEntryHandler>(this));

    httpServer_.Post("/login", std::make_shared<ChatLoginHandler>(this));
    httpServer_.Post("/register", std::make_shared<ChatRegisterHandler>(this));
    httpServer_.Post("/user/logout", std::make_shared<ChatLogoutHandler>(this));

    httpServer_.Get("/chat", std::make_shared<ChatHandler>(this));
    httpServer_.Post("/chat/send", std::make_shared<ChatSendHandler>(this));

    httpServer_.Get("/menu", std::make_shared<AIMenuHandler>(this));
    httpServer_.Get("/upload", std::make_shared<AIUploadHandler>(this));
    httpServer_.Post("/upload/send", std::make_shared<AIUploadSendHandler>(this));

    httpServer_.Post("/chat/history", std::make_shared<ChatHistoryHandler>(this));
    httpServer_.Post("/chat/send-new-session", std::make_shared<ChatCreateAndSendHandler>(this));
    httpServer_.Get("/chat/sessions", std::make_shared<ChatSessionsHandler>(this));

    httpServer_.Post("/chat/tts", std::make_shared<ChatSpeechHandler>(this));
}

// 创建内存存储并移动 unique_ptr，将管理器所有权交给 HttpServer。
void ChatServer::initializeSession() {
    auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();
    auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));
    setSessionManager(std::move(sessionManager));
}

// 只注册 CORS；这里没有额外注册登录认证或 Session 中间件。
void ChatServer::initializeMiddleware() {
    auto corsMiddleware = std::make_shared<http::middleware::CorsMiddleware>();
    httpServer_.addMiddleware(corsMiddleware);
}

// 用传入字段填写调用方响应；空响应指针只记录错误，设置失败时尝试生成 500。
void ChatServer::packageResp(const std::string& version,
    http::HttpResponse::HttpStatusCode statusCode,
    const std::string& statusMsg,
    bool close,
    const std::string& contentType,
    int contentLen,
    const std::string& body,
    http::HttpResponse* resp)
{
    if (resp == nullptr) {
        LOG_ERROR << "Response pointer is null";
        return;
    }

    try {
        resp->setVersion(version);
        resp->setStatusCode(statusCode);
        resp->setStatusMessage(statusMsg);
        resp->setCloseConnection(close);
        resp->setContentType(contentType);
        resp->setContentLength(contentLen);
        resp->setBody(body);
        LOG_INFO << "Response packaged successfully";
    }
    catch (const std::exception& e) {
        LOG_ERROR << "Error in packageResp: " << e.what();
        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setCloseConnection(true);
    }
}
