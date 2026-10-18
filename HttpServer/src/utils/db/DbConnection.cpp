// DbConnection.cpp：单条 MySQL 连接包装：创建连接、SQL 执行、有效性探测、重连和释放。
// 有参数查询使用 PreparedStatement；无参数查询使用 Statement；结果有效期依赖连接与语句对象。
// 执行函数持锁，但返回 ResultSet 后的遍历不在该锁内；不能把它视为完整并发查询隔离。

#include "../../../include/utils/db/DbConnection.h"

namespace http {
namespace db {

// 同步建立 Connector 连接并选择数据库；失败向上抛异常。
DbConnection::DbConnection(const std::string& host,
                           const std::string& user,
                           const std::string& password,
                           const std::string& database)
    : host_(host), user_(user), password_(password), database_(database)
{

    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    conn_.reset(driver->connect(host, user, password));
    conn_->setSchema(database);
}

DbConnection::~DbConnection()
{
    cleanup();
}

// 连接非空且 SELECT 1 探测成功才返回真；这里只在调用时检测。
bool DbConnection::isValid()
{
    return conn_ && ping();
}

// 先清理旧连接，再按保存凭据重建并选库；不是自动重试业务 SQL。
void DbConnection::reconnect()
{
    cleanup();
    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    conn_.reset(driver->connect(host_, user_, password_));
    conn_->setSchema(database_);
}

// 持锁关闭并重置连接；已有语句/结果生命周期也依赖这条连接，不应同时被其他路径使用。
void DbConnection::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_) {
        conn_->close();
        conn_.reset();
    }
}

// 执行 SELECT 1 并消费结果，SQL 异常返回 false；结果使用裸指针，未显式释放。
bool DbConnection::ping()
{
    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        stmt->execute("SELECT 1");

        sql::ResultSet* res = stmt->getResultSet();
        while (res->next()) {}
        return true;
    } catch (const sql::SQLException&) {
        return false;
    }
}

}
}
