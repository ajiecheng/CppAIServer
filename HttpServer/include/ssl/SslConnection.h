// SslConnection.h：单连接 TLS 适配尝试：拥有 SSL 和其 BIO，借用 SslContext，持有 TCP 连接共享引用。
// 当前构造使用内存 BIO，createCustomBioMethod/bioRead/bioWrite 并未接入这条构造路径。
// 握手输出未排空，已建立分支未把新密文写入 readBio，HTTP 响应也未统一经过 SSL_write；实现尚不完整。
// 回调重入、临时解密 Buffer 与成员缓冲区的衔接也需修正；不可将这些接口视为已验证 HTTPS。

#pragma once
#include "SslContext.h"
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/noncopyable.h>
#include <openssl/ssl.h>
#include <memory>

namespace ssl
{

using MessageCallback = std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
                                         muduo::net::Buffer*,
                                         muduo::Timestamp)>;

class SslConnection : muduo::noncopyable
{
public:
    using TcpConnectionPtr = std::shared_ptr<muduo::net::TcpConnection>;
    using BufferPtr = muduo::net::Buffer*;

    // 创建 SSL 和两个内存 BIO，SSL_set_bio 后由 SSL 管理其释放；然后覆盖 TCP 消息回调为 onRead。
    SslConnection(const TcpConnectionPtr& conn, SslContext* ctx);
    ~SslConnection();

    // 设服务端握手角色并尝试推进一次；ssl_ 创建失败时这里缺少空指针防护。
    void startHandshake();
    // 仅 ESTABLISHED 时执行一次 SSL_write，再取 writeBio 密文交给 TCP；未补发部分写入，也未处理 WANT_READ/WRITE 重试。
    void send(const void* data, size_t len);
    // 握手分支把输入密文写入 BIO；已建立分支直接 SSL_read，遗漏新密文注入，并把临时明文 Buffer 传给上层。
    void onRead(const TcpConnectionPtr& conn, BufferPtr buf, muduo::Timestamp time);
    bool isHandshakeCompleted() const { return state_ == SSLState::ESTABLISHED; }
    muduo::net::Buffer* getDecryptedBuffer() { return &decryptedBuffer_; }

    // 自定义 BIO 回调：借由 BIO data 取得包装对象并发送原字节；当前内存 BIO 未使用此回调。
    static int bioWrite(BIO* bio, const char* data, int len);
    // 从成员 readBuffer_ 拷贝并消费；无数据时返回 -1，但未设置 BIO retry 标记，且当前未接入。
    static int bioRead(BIO* bio, char* data, int len);
    // 仅对 FLUSH 返回成功，其他控制命令返回 0；不是完整 BIO 控制实现。
    static long bioCtrl(BIO* bio, int cmd, long num, void* ptr);

    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
private:
    // 尝试推进握手；WANT_READ/WRITE 仅等待，当前未把 writeBio 的握手密文发送出去。
    void handleHandshake();
    // 将传入字节加入成员写缓冲并交给 TCP；当前未见该辅助函数的调用链。
    void onEncrypted(const char* data, size_t len);
    // 向成员明文缓冲追加数据；当前 onRead 使用临时 Buffer，没有调用这里。
    void onDecrypted(const char* data, size_t len);
    // 把 SSL_get_error 结果映射为项目枚举；是否等待或关闭由调用方决定。
    SSLError getLastError(int ret);
    // WANT_READ/WRITE 保持等待，其他指定错误标记 ERROR 并 shutdown；当前主读写路径未统一调用。
    void handleError(SSLError error);

private:
    SSL*                ssl_;
    SslContext*         ctx_;
    TcpConnectionPtr    conn_;
    SSLState            state_;
    BIO*                readBio_;
    BIO*                writeBio_;
    muduo::net::Buffer  readBuffer_;
    muduo::net::Buffer  writeBuffer_;
    muduo::net::Buffer  decryptedBuffer_;
    MessageCallback     messageCallback_;
};

}
