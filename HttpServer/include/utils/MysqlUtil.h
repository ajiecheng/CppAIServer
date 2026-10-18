// MysqlUtil.h：数据库调用入口：从单例池取连接，再转发查询/更新及其参数。
// QueryResult 持有连接引用，让结果读取期间连接仍存活；res 是裸指针，没有独立释放逻辑。
// 当前没有自动回池动作，也没有跨多次调用共享事务连接的接口。

#pragma once
#include "db/DbConnectionPool.h"

#include <string>
#include <memory>

namespace http
{

struct QueryResult {
    // 共享连接所有权；没有自定义回池删除器。
    std::shared_ptr<db::DbConnection> conn;
    // 借用结果指针；包装类型未定义结果释放策略。
    sql::ResultSet* res;

    sql::ResultSet* operator->() { return res; }
    bool next() { return res->next(); }
};

class MysqlUtil
{
public:

    // 初始化全局池，参数包含连接地址、账号、数据库和预建数量。
    static void init(const std::string& host, const std::string& user,
                    const std::string& password, const std::string& database,
                    size_t poolSize = 10)
    {
        http::db::DbConnectionPool::getInstance().init(
            host, user, password, database, poolSize);
    }

    template<typename... Args>
    // 取连接执行后将连接与结果一起返回；共享所有权保证存活，不代表已经归还池。
    QueryResult executeQuery(const std::string& sql, Args&&... args)
    {
        auto conn = http::db::DbConnectionPool::getInstance().getConnection();
        sql::ResultSet* res = conn->executeQuery(sql, std::forward<Args>(args)...);
        return QueryResult{conn, res};
    }

    template<typename... Args>
    // 取连接执行并返回影响行数；局部共享指针释放时不自动回池。
    int executeUpdate(const std::string& sql, Args&&... args)
    {
        auto conn = http::db::DbConnectionPool::getInstance().getConnection();
        return conn->executeUpdate(sql, std::forward<Args>(args)...);
    }
};

}
