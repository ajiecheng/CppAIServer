// SslContext.h：SSL_CTX 的生命周期包装：建立服务端上下文、配置协议、加载证书私钥并设置会话缓存。
// initialize 返回成功/失败；析构释放 SSL_CTX。客户端认证、证书链及缓存大小配置未接入。
// 这里只准备公共 TLS 上下文；每条连接的 SSL 状态由 SslConnection 持有。

#pragma once
#include "SslConfig.h"
#include <openssl/ssl.h>
#include <memory>
#include <muduo/base/noncopyable.h>

namespace ssl
{

class SslContext : muduo::noncopyable
{
public:
    explicit SslContext(const SslConfig& config);
    ~SslContext();

    // 创建 SSL_CTX，依次设协议、加载证书和私钥、设缓存；失败返回 false，由拥有者析构释放上下文。
    bool initialize();
    SSL_CTX* getNativeHandle() { return ctx_; }

private:
    // 加载 PEM 证书和私钥并检查匹配；未加载配置中的证书链文件。
    bool loadCertificates();
    // 固定最低 TLS 1.2，并尝试设置传统密码套件；未使用 version_，也未检查设置返回值。
    bool setupProtocol();
    // 开启服务端缓存并设置超时；未应用 sessionCacheSize 配置。
    void setupSessionCache();
    // 记录说明并把 OpenSSL 错误队列打印到 stderr，不执行重试。
    static void handleSslError(const char* msg);

private:
    SSL_CTX*  ctx_;
    SslConfig config_;
};

}
