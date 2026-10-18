// SessionStorage.cpp：会话存储接口及内存实现：按 sessionId 保存共享会话对象，进程重启后数据消失。
// load 只清理本次访问到的过期项；没有全量定时清理，也没有 Redis 实现。
// 内存 map 和其中 Session 均未加锁，多 IO 线程访问时需要额外同步。

#include "../include/session/SessionStorage.h"
#include <iostream>

namespace http
{
namespace session
{

// 按 ID 保存同一个 Session 的 shared_ptr，同名覆盖，无深拷贝或磁盘持久化。
void MemorySessionStorage::save(std::shared_ptr<Session> session)
{
    sessions_[session->getId()] = session;
}

// 返回未过期共享对象；不存在/过期返回 nullptr，并删除本次找到的过期项。
std::shared_ptr<Session> MemorySessionStorage::load(const std::string& sessionId)
{
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end())
    {
        if (!it->second->isExpired())
        {
            return it->second;
        }
        else
        {

            sessions_.erase(it);
        }
    }
    return nullptr;
}

// 仅删除给定 ID 的 map 引用；其他 shared_ptr 仍可保持对象存活。
void MemorySessionStorage::remove(const std::string& sessionId)
{
    sessions_.erase(sessionId);
}

}
}
