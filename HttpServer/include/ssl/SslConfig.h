// SslConfig.h：TLS 配置值对象：证书/私钥路径、密码套件及会话缓存等参数的存取。
// SslContext 当前只应用证书、私钥、cipherList 和超时，并固定最低 TLS 1.2；其余字段未全部使用。

#pragma once
#include "SslTypes.h"
#include <string>
#include <vector>

namespace ssl
{

class SslConfig
{
public:
    SslConfig();
    ~SslConfig() = default;

    void setCertificateFile(const std::string& certFile) { certFile_ = certFile; }
    void setPrivateKeyFile(const std::string& keyFile) { keyFile_ = keyFile; }
    void setCertificateChainFile(const std::string& chainFile) { chainFile_ = chainFile; }

    void setProtocolVersion(SSLVersion version) { version_ = version; }
    void setCipherList(const std::string& cipherList) { cipherList_ = cipherList; }

    void setVerifyClient(bool verify) { verifyClient_ = verify; }
    void setVerifyDepth(int depth) { verifyDepth_ = depth; }

    void setSessionTimeout(int seconds) { sessionTimeout_ = seconds; }
    void setSessionCacheSize(long size) { sessionCacheSize_ = size; }

    const std::string& getCertificateFile() const { return certFile_; }
    const std::string& getPrivateKeyFile() const { return keyFile_; }
    const std::string& getCertificateChainFile() const { return chainFile_; }
    SSLVersion getProtocolVersion() const { return version_; }
    const std::string& getCipherList() const { return cipherList_; }
    bool getVerifyClient() const { return verifyClient_; }
    int getVerifyDepth() const { return verifyDepth_; }
    int getSessionTimeout() const { return sessionTimeout_; }
    long getSessionCacheSize() const { return sessionCacheSize_; }

private:
    std::string certFile_;
    std::string keyFile_;
    std::string chainFile_;
    SSLVersion  version_;
    std::string cipherList_;
    bool        verifyClient_;
    int         verifyDepth_;
    int         sessionTimeout_;
    long        sessionCacheSize_;
};

}
