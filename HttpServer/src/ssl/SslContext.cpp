// SslContext.cpp：SSL_CTX 的生命周期包装：建立服务端上下文、配置协议、加载证书私钥并设置会话缓存。
// initialize 返回成功/失败；析构释放 SSL_CTX。客户端认证、证书链及缓存大小配置未接入。
// 这里只准备公共 TLS 上下文；每条连接的 SSL 状态由 SslConnection 持有。

#include "../../include/ssl/SslContext.h"
#include <openssl/err.h>
#include <muduo/base/Logging.h>

namespace ssl {

SslContext::SslContext(const SslConfig& config)
    : ctx_(nullptr), config_(config)
{
}

SslContext::~SslContext()
{
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

// 创建 SSL_CTX，依次设协议、加载证书和私钥、设缓存；失败返回 false，由拥有者析构释放上下文。
bool SslContext::initialize()
{
    ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ctx_) {
        handleSslError("SSL_CTX_new failed");
        return false;
    }
    if (!setupProtocol()) return false;
    if (!loadCertificates()) return false;
    setupSessionCache();
    return true;
}

// 加载 PEM 证书和私钥并检查匹配；未加载配置中的证书链文件。
bool SslContext::loadCertificates()
{
    if (SSL_CTX_use_certificate_file(ctx_, config_.getCertificateFile().c_str(), SSL_FILETYPE_PEM) <= 0) {
        handleSslError("Failed to load certificate file");
        return false;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx_, config_.getPrivateKeyFile().c_str(), SSL_FILETYPE_PEM) <= 0) {
        handleSslError("Failed to load private key file");
        return false;
    }
    if (!SSL_CTX_check_private_key(ctx_)) {
        handleSslError("Private key does not match certificate");
        return false;
    }
    return true;
}

// 固定最低 TLS 1.2，并尝试设置传统密码套件；未使用 version_，也未检查设置返回值。
bool SslContext::setupProtocol()
{
    SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
    if (!config_.getCipherList().empty()) {
        SSL_CTX_set_cipher_list(ctx_, config_.getCipherList().c_str());
    }
    return true;
}

// 开启服务端缓存并设置超时；未应用 sessionCacheSize 配置。
void SslContext::setupSessionCache()
{
    SSL_CTX_set_session_cache_mode(ctx_, SSL_SESS_CACHE_SERVER);
    SSL_CTX_set_timeout(ctx_, config_.getSessionTimeout());
}

// 记录说明并把 OpenSSL 错误队列打印到 stderr，不执行重试。
void SslContext::handleSslError(const char* msg)
{
    LOG_ERROR << msg;
    ERR_print_errors_fp(stderr);
}

}
