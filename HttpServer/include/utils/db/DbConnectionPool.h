// DbConnectionPool.h：进程级数据库连接池对象：初始化预建连接，获取时出队，池空时直接创建。
// 当前 returnConnection 未接入 MysqlUtil 的使用路径，普通 shared_ptr 析构不自动归还连接。
// 无容量上限、等待超时或运行中的健康检查；cv_ 只有通知，没有对应等待。

#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include "DbConnection.h"

namespace http
{
namespace db
{

class DbConnectionPool
{
public:

    static DbConnectionPool& getInstance()
    {
        static DbConnectionPool instance;
        return instance;
    }

    // 持池锁同步预建 poolSize 条连接；已初始化则直接返回，初始化异常没有完整回滚。
    void init(const std::string& host,
             const std::string& user,
             const std::string& password,
             const std::string& database,
             size_t poolSize = 10);

    // 持锁出队并 ping/重连；池空时也在持锁期间同步创建，无上限与等待机制。
    std::shared_ptr<DbConnection> getConnection();

    // 显式入队并 notify_one；当前业务路径未调用，不能依靠 shared_ptr 自动触发。
    void returnConnection(std::shared_ptr<DbConnection> conn);

private:
    DbConnectionPool();
    ~DbConnectionPool();

    DbConnectionPool(const DbConnectionPool&) = delete;
    DbConnectionPool& operator=(const DbConnectionPool&) = delete;

    // 普通 make_shared 创建连接，没有回池自定义 deleter。
    std::shared_ptr<DbConnection> createConnection();
    // 预留空实现；checkThread_ 未启动，不存在定期健康检查。
    void checkConnections();

private:
    std::string host_, user_, password_, database_;
    // 空闲连接队列；get 出队，只有显式 return 才入队。
    std::queue<std::shared_ptr<DbConnection>> connections_;
    std::mutex mutex_;
    // 预留通知对象；当前没有 wait，notify_one 不会实现等待池化。
    std::condition_variable cv_;
    bool initialized_ = false;
    // 预留线程成员，当前未启动。
    std::thread checkThread_;
};

}
}
