// SslConnection.cpp：单连接 TLS 适配尝试：拥有 SSL 和其 BIO，借用 SslContext，持有 TCP 连接共享引用。
// 当前构造使用内存 BIO，createCustomBioMethod/bioRead/bioWrite 并未接入这条构造路径。
// 握手输出未排空，已建立分支未把新密文写入 readBio，HTTP 响应也未统一经过 SSL_write；实现尚不完整。
// 回调重入、临时解密 Buffer 与成员缓冲区的衔接也需修正；不可将这些接口视为已验证 HTTPS。

#include "../../include/ssl/SslConnection.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>

namespace ssl
{

// 创建自定义 BIO 方法表的辅助函数；当前构造使用 BIO_s_mem，此函数未调用。
static BIO_METHOD* createCustomBioMethod()
{
    BIO_METHOD* method = BIO_meth_new(BIO_TYPE_MEM, "custom");
    BIO_meth_set_write(method, SslConnection::bioWrite);
    BIO_meth_set_read(method, SslConnection::bioRead);
    BIO_meth_set_ctrl(method, SslConnection::bioCtrl);
    return method;
}

// 创建 SSL 和两个内存 BIO，SSL_set_bio 后由 SSL 管理其释放；然后覆盖 TCP 消息回调为 onRead。
SslConnection::SslConnection(const TcpConnectionPtr& conn, SslContext* ctx)
    : ssl_(nullptr)
    , ctx_(ctx)
    , conn_(conn)
    , state_(SSLState::HANDSHAKE)
    , readBio_(nullptr)
    , writeBio_(nullptr)
    , messageCallback_(nullptr)
{

    ssl_ = SSL_new(ctx_->getNativeHandle());
    if (!ssl_) {
        LOG_ERROR << "Failed to create SSL object: " << ERR_error_string(ERR_get_error(), nullptr);
        return;
    }

    readBio_ = BIO_new(BIO_s_mem());
    writeBio_ = BIO_new(BIO_s_mem());

    if (!readBio_ || !writeBio_) {
        LOG_ERROR << "Failed to create BIO objects";
        SSL_free(ssl_);
        ssl_ = nullptr;
        return;
    }

    // 成功绑定后 BIO 随 SSL_free 释放；前面分配失败时单独已成功的 BIO 未全部清理。
    SSL_set_bio(ssl_, readBio_, writeBio_);
    SSL_set_accept_state(ssl_);

    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);

    conn_->setMessageCallback(
        std::bind(&SslConnection::onRead, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3));
}

SslConnection::~SslConnection()
{
    if (ssl_)
    {
        SSL_free(ssl_);
    }
}

// 设服务端握手角色并尝试推进一次；ssl_ 创建失败时这里缺少空指针防护。
void SslConnection::startHandshake()
{
    SSL_set_accept_state(ssl_);
    handleHandshake();
}

// 仅 ESTABLISHED 时执行一次 SSL_write，再取 writeBio 密文交给 TCP；未补发部分写入，也未处理 WANT_READ/WRITE 重试。
void SslConnection::send(const void* data, size_t len)
{
    if (state_ != SSLState::ESTABLISHED) {
        LOG_ERROR << "Cannot send data before SSL handshake is complete";
        return;
    }

    int written = SSL_write(ssl_, data, len);
    if (written <= 0) {
        int err = SSL_get_error(ssl_, written);
        LOG_ERROR << "SSL_write failed: " << ERR_error_string(err, nullptr);
        return;
    }

    char buf[4096];
    int pending;
    while ((pending = BIO_pending(writeBio_)) > 0) {
        int bytes = BIO_read(writeBio_, buf,
                           std::min(pending, static_cast<int>(sizeof(buf))));
        if (bytes > 0) {
            conn_->send(buf, bytes);
        }
    }
}

// 握手分支把输入密文写入 BIO；已建立分支直接 SSL_read，遗漏新密文注入，并把临时明文 Buffer 传给上层。
void SslConnection::onRead(const TcpConnectionPtr& conn, BufferPtr buf,
                         muduo::Timestamp time)
{
    if (state_ == SSLState::HANDSHAKE) {

        BIO_write(readBio_, buf->peek(), buf->readableBytes());
        buf->retrieve(buf->readableBytes());
        handleHandshake();
        return;
    } else if (state_ == SSLState::ESTABLISHED) {

        char decryptedData[4096];
        int ret = SSL_read(ssl_, decryptedData, sizeof(decryptedData));
        if (ret > 0) {

            // 局部明文 Buffer 只活到本分支结束，不是成员 decryptedBuffer_。
            muduo::net::Buffer decryptedBuffer;
            decryptedBuffer.append(decryptedData, ret);

            if (messageCallback_) {
                messageCallback_(conn, &decryptedBuffer, time);
            }
        }
    }
}

// 尝试推进握手；WANT_READ/WRITE 仅等待，当前未把 writeBio 的握手密文发送出去。
void SslConnection::handleHandshake()
{
    int ret = SSL_do_handshake(ssl_);

    if (ret == 1) {
        state_ = SSLState::ESTABLISHED;
        LOG_INFO << "SSL handshake completed successfully";
        LOG_INFO << "Using cipher: " << SSL_get_cipher(ssl_);
        LOG_INFO << "Protocol version: " << SSL_get_version(ssl_);

        if (!messageCallback_) {
            LOG_WARN << "No message callback set after SSL handshake";
        }
        return;
    }

    int err = SSL_get_error(ssl_, ret);
    switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:

            break;

        default: {

            char errBuf[256];
            unsigned long errCode = ERR_get_error();
            ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
            LOG_ERROR << "SSL handshake failed: " << errBuf;
            conn_->shutdown();
            break;
        }
    }
}

// 将传入字节加入成员写缓冲并交给 TCP；当前未见该辅助函数的调用链。
void SslConnection::onEncrypted(const char* data, size_t len)
{
    writeBuffer_.append(data, len);
    conn_->send(&writeBuffer_);
}

// 向成员明文缓冲追加数据；当前 onRead 使用临时 Buffer，没有调用这里。
void SslConnection::onDecrypted(const char* data, size_t len)
{
    decryptedBuffer_.append(data, len);
}

// 把 SSL_get_error 结果映射为项目枚举；是否等待或关闭由调用方决定。
SSLError SslConnection::getLastError(int ret)
{
    int err = SSL_get_error(ssl_, ret);
    switch (err)
    {
        case SSL_ERROR_NONE:
            return SSLError::NONE;
        case SSL_ERROR_WANT_READ:
            return SSLError::WANT_READ;
        case SSL_ERROR_WANT_WRITE:
            return SSLError::WANT_WRITE;
        case SSL_ERROR_SYSCALL:
            return SSLError::SYSCALL;
        case SSL_ERROR_SSL:
            return SSLError::SSL;
        default:
            return SSLError::UNKNOWN;
    }
}

// WANT_READ/WRITE 保持等待，其他指定错误标记 ERROR 并 shutdown；当前主读写路径未统一调用。
void SslConnection::handleError(SSLError error)
{
    switch (error)
    {
        case SSLError::WANT_READ:
        case SSLError::WANT_WRITE:

            break;
        case SSLError::SSL:
        case SSLError::SYSCALL:
        case SSLError::UNKNOWN:
            LOG_ERROR << "SSL error occurred: " << ERR_error_string(ERR_get_error(), nullptr);
            state_ = SSLState::ERROR;
            conn_->shutdown();
            break;
        default:
            break;
    }
}

// 自定义 BIO 回调：借由 BIO data 取得包装对象并发送原字节；当前内存 BIO 未使用此回调。
int SslConnection::bioWrite(BIO* bio, const char* data, int len)
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if (!conn) return -1;

    conn->conn_->send(data, len);
    return len;
}

// 从成员 readBuffer_ 拷贝并消费；无数据时返回 -1，但未设置 BIO retry 标记，且当前未接入。
int SslConnection::bioRead(BIO* bio, char* data, int len)
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if (!conn) return -1;

    size_t readable = conn->readBuffer_.readableBytes();
    if (readable == 0)
    {
        return -1;
    }

    size_t toRead = std::min(static_cast<size_t>(len), readable);
    memcpy(data, conn->readBuffer_.peek(), toRead);
    conn->readBuffer_.retrieve(toRead);
    return toRead;
}

// 仅对 FLUSH 返回成功，其他控制命令返回 0；不是完整 BIO 控制实现。
long SslConnection::bioCtrl(BIO* bio, int cmd, long num, void* ptr)
{
    switch (cmd)
    {
        case BIO_CTRL_FLUSH:
            return 1;
        default:
            return 0;
    }
}

}
