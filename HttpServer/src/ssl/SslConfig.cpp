// SslConfig.cpp：TLS 配置值对象：证书/私钥路径、密码套件及会话缓存等参数的存取。
// SslContext 当前只应用证书、私钥、cipherList 和超时，并固定最低 TLS 1.2；其余字段未全部使用。

#include "../../include/ssl/SslConfig.h"

namespace ssl
{
SslConfig::SslConfig()
    : version_(SSLVersion::TLS_1_2)
    // 保留原配置字符串；其中 MDS 是否为有效选择项需要按 OpenSSL 版本核验，不替代码修正。
    , cipherList_("HIGH:!aNULL:!MDS")
    , verifyClient_(false)
    , verifyDepth_(4)
    , sessionTimeout_(300)
    , sessionCacheSize_(20480L)
{
}

};
