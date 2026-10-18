// DbConnection.h：单条 MySQL 连接包装：创建连接、SQL 执行、有效性探测、重连和释放。
// 有参数查询使用 PreparedStatement；无参数查询使用 Statement；结果有效期依赖连接与语句对象。
// 执行函数持锁，但返回 ResultSet 后的遍历不在该锁内；不能把它视为完整并发查询隔离。

#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <mysql_driver.h>
#include <mysql/mysql.h>
#include <muduo/base/Logging.h>
#include "DbException.h"

namespace http
{
namespace db
{

class DbConnection
{
public:
    // 同步建立 Connector 连接并选择数据库；失败向上抛异常。
    DbConnection(const std::string& host,
                const std::string& user,
                const std::string& password,
                const std::string& database);
    ~DbConnection();

    // 同步建立 Connector 连接并选择数据库；失败向上抛异常。
    DbConnection(const DbConnection&) = delete;
    DbConnection& operator=(const DbConnection&) = delete;

    // 连接非空且 SELECT 1 探测成功才返回真；这里只在调用时检测。
    bool isValid();
    // 先清理旧连接，再按保存凭据重建并选库；不是自动重试业务 SQL。
    void reconnect();
    // 持锁关闭并重置连接；已有语句/结果生命周期也依赖这条连接，不应同时被其他路径使用。
    void cleanup();
    // 执行 SELECT 1 并消费结果，SQL 异常返回 false；结果使用裸指针，未显式释放。
    bool ping();

    template<typename... Args>
    // 持连接锁执行 SQL；返回结果时保留语句成员和连接引用，结果遍历阶段不再持锁。
    sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try
        {
            if constexpr (sizeof...(Args) == 0) {

                std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
                stmt->execute(sql);
                stmt_ = std::move(stmt);
                return stmt_->getResultSet();
            } else {

                std::unique_ptr<sql::PreparedStatement> pstmt(conn_->prepareStatement(sql));
                // 从 1 开始递归绑定占位符；数值转字符串后 setString，字符串重载直接绑定，类型支持以重载匹配为准。
                bindParams(pstmt.get(), 1, std::forward<Args>(args)...);
                sql::ResultSet* r = pstmt->executeQuery();
                pstmt_ = std::move(pstmt);
                return r;
            }
        }
        catch (const sql::SQLException& e)
        {
            LOG_ERROR << "Query failed: " << e.what() << ", SQL: " << sql;
            throw DbException(e.what());
        }
    }

    template<typename... Args>
    // 使用 PreparedStatement 执行并返回影响行数；没有自动重试、显式事务或回池。
    int executeUpdate(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try
        {
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn_->prepareStatement(sql));
            // 从 1 开始递归绑定占位符；数值转字符串后 setString，字符串重载直接绑定，类型支持以重载匹配为准。
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            return stmt->executeUpdate();
        }
        catch (const sql::SQLException& e)
        {
            LOG_ERROR << "Update failed: " << e.what() << ", SQL: " << sql;
            throw DbException(e.what());
        }
    }

private:

    // 从 1 开始递归绑定占位符；数值转字符串后 setString，字符串重载直接绑定，类型支持以重载匹配为准。
    void bindParams(sql::PreparedStatement*, int) {}

    template<typename T, typename... Args>
    // 从 1 开始递归绑定占位符；数值转字符串后 setString，字符串重载直接绑定，类型支持以重载匹配为准。
    void bindParams(sql::PreparedStatement* stmt, int index,
                   T&& value, Args&&... args)
    {
        stmt->setString(index, std::to_string(std::forward<T>(value)));
        // 从 1 开始递归绑定占位符；数值转字符串后 setString，字符串重载直接绑定，类型支持以重载匹配为准。
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    template<typename... Args>
    // 从 1 开始递归绑定占位符；数值转字符串后 setString，字符串重载直接绑定，类型支持以重载匹配为准。
    void bindParams(sql::PreparedStatement* stmt, int index,
                   const std::string& value, Args&&... args)
    {
        stmt->setString(index, value);
        // 从 1 开始递归绑定占位符；数值转字符串后 setString，字符串重载直接绑定，类型支持以重载匹配为准。
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

private:
    std::shared_ptr<sql::Connection> conn_;
    // 保存最近无参数查询的语句，避免函数返回立刻销毁语句。
    std::unique_ptr<sql::Statement>  stmt_;
    // 保存最近参数化查询的语句；下一次覆盖会影响先前结果的有效期。
    std::unique_ptr<sql::PreparedStatement> pstmt_;
    std::string host_, user_, password_, database_;
    std::mutex mutex_;
};

}
}
