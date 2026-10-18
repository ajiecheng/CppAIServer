// SessionManager.cpp：Cookie 与服务端会话的桥梁：从请求 Cookie 取 sessionId，加载或新建 Session 并续期保存。
// 只有新建 Session 时设置 Set-Cookie；已有 Cookie 对应失效会话时同样会新建。
// 不负责查询账号密码；Handler 决定何时调用。随机数生成器和内存存储均未做并发保护。

#include"../include/session/SessionManager.h"
#include <iomanip>
#include <iostream>
#include <sstream>

namespace http
{
namespace session
{

SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage)
    : storage_(std::move(storage))
    , rng_(std::random_device{}())
{}

// Cookie → load → 失效则新建并 Set-Cookie → refresh → save；不会自动设置 isLoggedIn。
std::shared_ptr<Session> SessionManager::getSession(const HttpRequest& req, HttpResponse* resp)
{
    std::string sessionId = getSessionIdFromCookie(req);
    std::shared_ptr<Session> session;

    if (!sessionId.empty())
    {
        session = storage_->load(sessionId);
    }

    if (!session || session->isExpired())
    {
        // 没有有效会话才生成新 ID；与用户账号密码校验是独立步骤。
        sessionId = generateSessionId();
        session = std::make_shared<Session>(sessionId, this);
        setSessionCookie(sessionId, resp);
    }
    else
    {
        session->setManager(this);
    }

    session->refresh();
    // 保存服务端对象；浏览器只获得标识，不接收此对象的数据 map。
    storage_->save(session);
    return session;
}

// 用 mt19937 生成 32 个十六进制字符；不是密码学安全 token，存储中也未检查碰撞。
std::string SessionManager::generateSessionId()
{
    std::stringstream ss;
    std::uniform_int_distribution<> dist(0, 15);

    for (int i = 0; i < 32; ++i)
    {
        ss << std::hex << dist(rng_);
    }
    return ss.str();
}

// 删除服务端存储项；不会发送过期 Cookie，也不关闭 TCP 连接。
void SessionManager::destroySession(const std::string& sessionId)
{
    storage_->remove(sessionId);
}

// 当前为空函数；过期项只在内存存储 load 访问到时清理。
void SessionManager::cleanExpiredSessions()
{

}

// 查找 sessionId= 子串并读至分号；未严格按 Cookie 键边界分词。
std::string SessionManager::getSessionIdFromCookie(const HttpRequest& req)
{
    std::string sessionId;
    std::string cookie = req.getHeader("Cookie");

    if (!cookie.empty())
    {
        size_t pos = cookie.find("sessionId=");
        if (pos != std::string::npos)
        {
            pos += 10;
            size_t end = cookie.find(';', pos);
            if (end != std::string::npos)
            {
                sessionId = cookie.substr(pos, end - pos);
            }
            else
            {
                sessionId = cookie.substr(pos);
            }
        }
    }

    return sessionId;
}

// 仅向响应添加 sessionId、Path=/、HttpOnly；没有 Secure、SameSite 或 Max-Age。
void SessionManager::setSessionCookie(const std::string& sessionId, HttpResponse* resp)
{
    std::string cookie = "sessionId=" + sessionId + "; Path=/; HttpOnly";
    resp->addHeader("Set-Cookie", cookie);
}

}
}
