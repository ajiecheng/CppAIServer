// ChatServer.h：业务装配层：拥有 HttpServer，初始化数据库、内存登录会话、CORS 和 14 条方法/路径映射。
// 请求链：HttpServer → Router → Handler；Handler 借用 ChatServer 访问业务共享状态。
// chatInformation 按用户 ID、聊天会话 ID 索引 AIHelper；这与 Cookie 登录 Session 是两套状态。
// 共享容器的锁不自动保护容器中 AIHelper 的内部数据；各调用点仍需核对同步范围。

#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include "../../../HttpServer/include/http/HttpServer.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../../../HttpServer/include/utils/FileUtil.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"

#include"AIUtil/AISpeechProcessor.h"
#include"AIUtil/AIHelper.h"
#include"AIUtil/ImageRecognizer.h"
#include"AIUtil/base64.h"
#include"AIUtil/MQManager.h"

class ChatLoginHandler;
class ChatRegisterHandler;
class ChatLogoutHandler;
class ChatHandler;
class ChatEntryHandler;
class ChatSendHandler;
class ChatHistoryHandler;
class AIMenuHandler;
class AIUploadHandler;
class AIUploadSendHandler;
class ChatCreateAndSendHandler;
class ChatSessionsHandler;
class ChatSpeechHandler;

class ChatServer {
public:
    // 先构造成员，再执行 initialize。注意：第 3 个实参 option 实际传给 HttpServer 的 bool useSSL，TCP option 未正确转发。
    ChatServer(int port,
        const std::string& name,
        muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);

    // 向 HTTP/muduo 层转发线程数配置，这一句本身不负责启动工作线程。
    void setThreadNum(int numThreads);
    // 转交 HTTP 层启动；正常运行期间主线程停留在其事件循环中。
    void start();
    // 启动阶段读取历史，仅恢复内存，不重新发布写库消息。
    void initChatMessage();

private:

    friend class ChatLoginHandler;
    friend class ChatRegisterHandler;
    friend ChatLogoutHandler;
    friend class ChatHandler;
    friend class ChatEntryHandler;
    friend class ChatSendHandler;
    friend class AIMenuHandler;
    friend class AIUploadHandler;
    friend class AIUploadSendHandler;
    friend class ChatHistoryHandler;
    friend class ChatCreateAndSendHandler;
    friend class ChatSessionsHandler;
    friend class ChatSpeechHandler;

private:

    // 同步初始化数据库池 → 内存 Session → CORS → 路由；失败会影响后续装配，不是后台异步初始化。
    void initialize();
    // 创建内存存储并移动 unique_ptr，将管理器所有权交给 HttpServer。
    void initializeSession();
    // 在启动阶段注册方法和精确路径，共 14 条映射；业务当前未注册动态路径。
    void initializeRouter();
    // 只注册 CORS；这里没有额外注册登录认证或 Session 中间件。
    void initializeMiddleware();

    // 按时间和 id 遍历 chat_message，依 id/session_id 创建或复用 AIHelper；读取了 is_user，但 restoreMessage 未保存角色。
    void readDataFromMySQL();

    // 用传入字段填写调用方响应；空响应指针只记录错误，设置失败时尝试生成 500。
    void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode statusCode,
        const std::string& statusMsg, bool close, const std::string& contentType,
        int contentLen, const std::string& body, http::HttpResponse* resp);

    void setSessionManager(std::unique_ptr<http::session::SessionManager> manager)
    {
        httpServer_.setSessionManager(std::move(manager));
    }
    http::session::SessionManager* getSessionManager() const
    {
        return httpServer_.getSessionManager();
    }

    // 拥有 HTTP 服务对象；其内部再持有 muduo 网络对象。
    http::HttpServer httpServer_;

    http::MysqlUtil mysqlUtil_;

    // 用户在线标记，与 TCP 连接是否还存在并非同一状态。
    std::unordered_map<int, bool> onlineUsers_;
    std::mutex mutexForOnlineUsers_;

    // 用户 ID → 聊天会话 ID → AIHelper；shared_ptr 只保证对象存活，不保证内部线程安全。
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<AIHelper>>> chatInformation;
    std::mutex mutexForChatInformation;

    std::unordered_map<int, std::shared_ptr<ImageRecognizer>> ImageRecognizerMap;
    std::mutex mutexForImageRecognizerMap;

    // 聊天会话列表，供页面列出会话；读取和新增的锁当前不统一。
    std::unordered_map<int, std::vector<std::string>> sessionsIdsMap;
    std::mutex mutexForSessionsId;
};
