// DbConnectionPool.cpp：进程级数据库连接池对象：初始化预建连接，获取时出队，池空时直接创建。
// 当前 returnConnection 未接入 MysqlUtil 的使用路径，普通 shared_ptr 析构不自动归还连接。
// 无容量上限、等待超时或运行中的健康检查；cv_ 只有通知，没有对应等待。

#include "../../../include/utils/db/DbConnectionPool.h"
#include <muduo/base/Logging.h>

namespace http {
namespace db {

DbConnectionPool::DbConnectionPool()
    : initialized_(false)
{
}

DbConnectionPool::~DbConnectionPool()
{
    if (checkThread_.joinable()) {
        checkThread_.join();
    }
}

// 持池锁同步预建 poolSize 条连接；已初始化则直接返回，初始化异常没有完整回滚。
void DbConnectionPool::init(const std::string& host,
                            const std::string& user,
                            const std::string& password,
                            const std::string& database,
                            size_t poolSize)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;

    host_ = host;
    user_ = user;
    password_ = password;
    database_ = database;

    for (size_t i = 0; i < poolSize; ++i) {
        auto conn = createConnection();
        connections_.push(conn);
    }

    initialized_ = true;
}

// 持锁出队并 ping/重连；池空时也在持锁期间同步创建，无上限与等待机制。
std::shared_ptr<DbConnection> DbConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> lock(mutex_);

    if (connections_.empty()) {

        return createConnection();
    }

    auto conn = connections_.front();
    connections_.pop();

    if (!conn->isValid()) {
        conn->reconnect();
    }

    return conn;
}

// 显式入队并 notify_one；当前业务路径未调用，不能依靠 shared_ptr 自动触发。
void DbConnectionPool::returnConnection(std::shared_ptr<DbConnection> conn)
{
    if (!conn) return;
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.push(std::move(conn));
    cv_.notify_one();
}

// 普通 make_shared 创建连接，没有回池自定义 deleter。
std::shared_ptr<DbConnection> DbConnectionPool::createConnection()
{
    return std::make_shared<DbConnection>(host_, user_, password_, database_);
}

// 预留空实现；checkThread_ 未启动，不存在定期健康检查。
void DbConnectionPool::checkConnections()
{

}

}
}
