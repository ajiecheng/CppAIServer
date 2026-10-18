// SessionManager.h：Cookie 与服务端会话的桥梁：从请求 Cookie 取 sessionId，加载或新建 Session 并续期保存。
// 只有新建 Session 时设置 Set-Cookie；已有 Cookie 对应失效会话时同样会新建。
// 不负责查询账号密码；Handler 决定何时调用。随机数生成器和内存存储均未做并发保护。

#pragma once

#include "SessionStorage.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"
#include <memory>
#include <random>

namespace http
{
namespace session
{

class SessionManager
{
public:
    explicit SessionManager(std::unique_ptr<SessionStorage> storage);

    // Cookie → load → 失效则新建并 Set-Cookie → refresh → save；不会自动设置 isLoggedIn。
    std::shared_ptr<Session> getSession(const HttpRequest& req, HttpResponse* resp);

    // 删除服务端存储项；不会发送过期 Cookie，也不关闭 TCP 连接。
    void destroySession(const std::string& sessionId);
    // 当前为空函数；过期项只在内存存储 load 访问到时清理。
    void cleanExpiredSessions();

    // 转发给存储 save；存储实现决定持久化方式，本项目当前为内存。
    void updateSession(std::shared_ptr<Session> session)
    {
        storage_->save(session);
    }

private:

    // 用 mt19937 生成 32 个十六进制字符；不是密码学安全 token，存储中也未检查碰撞。
    std::string generateSessionId();

    // 查找 sessionId= 子串并读至分号；未严格按 Cookie 键边界分词。
    std::string getSessionIdFromCookie(const HttpRequest& req);

    // 仅向响应添加 sessionId、Path=/、HttpOnly；没有 Secure、SameSite 或 Max-Age。
    void setSessionCookie(const std::string& sessionId, HttpResponse* resp);

private:
    std::unique_ptr<SessionStorage> storage_;
    std::mt19937 rng_;
};

}
}
