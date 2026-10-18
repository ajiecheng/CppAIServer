// DbException.h：数据库异常类型：以 runtime_error 携带错误说明，供 DbConnection 转换 Connector 异常。
// 异常传播到调用方，不代表本层已经重试或回滚业务操作。

#pragma once
#include <stdexcept>
#include <string>

namespace http {
namespace db {

class DbException : public std::runtime_error
{
public:
    explicit DbException(const std::string& message)
        : std::runtime_error(message) {}

    explicit DbException(const char* message)
        : std::runtime_error(message) {}
};

}
}
