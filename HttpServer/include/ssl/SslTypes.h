// SslTypes.h：TLS 配置版本、连接状态和错误类别的枚举；这是项目内类型，不是完整的 OpenSSL 状态机。
// 部分枚举值当前没有对应状态转换或配置应用，不能仅凭声明认定功能生效。

#pragma once
#include <string>

namespace ssl
{

enum class SSLVersion
{
    TLS_1_0,
    TLS_1_1,
    TLS_1_2,
    TLS_1_3
};

enum class SSLError
{
    NONE,
    WANT_READ,
    WANT_WRITE,
    SYSCALL,
    SSL,
    UNKNOWN
};

enum class SSLState
{
    HANDSHAKE,
    ESTABLISHED,
    SHUTDOWN,
    ERROR
};

}
