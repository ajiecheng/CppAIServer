// Session.h：服务端登录会话：以字符串键值保存身份信息，过期时间独立于 TCP 连接。
// SessionManager/Storage 以 shared_ptr 共享会话；sessionManager_ 是不拥有管理器的借用指针。
// setValue 通过 shared_from_this 回存，要求对象已经由 shared_ptr 管理；成员数据未加锁。

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

namespace http
{

namespace session
{

class SessionManager;

class Session : public std::enable_shared_from_this<Session>
{
public:

    // 记录 ID、寿命秒数和管理器借用指针，refresh 初始化过期时间。
    Session(const std::string& sessionId, SessionManager* sessionManager, int maxAge = 3600);

    const std::string& getId() const { return sessionId_; }

    // 以 system_clock 当前时间比较截止时间；系统时钟调整可能影响判断，不能保证单调。
    bool isExpired() const;
    // 把过期点设置为现在加 maxAge 秒；只有调用路径实际刷新时才续期。
    void refresh();

    void setManager(SessionManager* sessionManager) { sessionManager_ = sessionManager; }
    SessionManager* getManager() const { return sessionManager_; }

    // 修改键值后回存 shared_ptr；必须已有共享所有权，不可在裸对象/构造期直接依赖 shared_from_this。
    void setValue(const std::string& key, const std::string& value);
    // 不存在时返回空串，因此无法仅用返回值区分不存在和已保存空值。
    std::string getValue(const std::string& key) const;
    // 从当前会话删除键；本函数不调用 updateSession，内存存储因共享同对象可见修改。
    void remove(const std::string& key);
    // 清空会话数据，不删除存储索引；登出另外调用 destroySession。
    void clear();

private:
    std::string                                  sessionId_;
    // 登录身份等字符串数据；没有内置互斥锁。
    std::unordered_map<std::string, std::string> data_;
    std::chrono::system_clock::time_point        expiryTime_;
    int                                          maxAge_;
    // 借用管理器，避免 Session 与管理器之间形成共享所有权环。
    SessionManager*                              sessionManager_;
};

}
}
