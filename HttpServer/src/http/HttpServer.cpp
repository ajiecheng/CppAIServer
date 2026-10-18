// HttpServer.cpp：HTTP 框架与 muduo 的适配层：注册连接/消息回调，组织解析、分发、响应发送。
// 普通 HTTP 主链：onConnection 配置连接级 HttpContext → onMessage → onRequest → handleRequest。
// handleRequest 按前置中间件、路由、后置中间件运行；Session 由具体 Handler 按需访问。
// TLS 是条件分支，当前接入存在缺口；本文件不代表 HTTPS 已完成联调。

#include "../../include/http/HttpServer.h"

#include <any>
#include <functional>
#include <memory>

namespace http
{

// 默认 404 辅助回调；构造函数实际把 httpCallback_ 绑定到 handleRequest。
void defaultHttpCallback(const HttpRequest &, HttpResponse *resp)
{
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
}

// 配置监听地址、TCP 服务与请求回调。注意成员按头文件声明顺序构造：server_ 早于 mainLoop_，存在依赖顺序风险。
HttpServer::HttpServer(int port,
                       const std::string &name,
                       bool useSSL,
                       muduo::net::TcpServer::Option option)
    : listenAddr_(port)
    , server_(&mainLoop_, listenAddr_, name, option)
    , useSSL_(useSSL)
    , httpCallback_(std::bind(&HttpServer::handleRequest, this,
                              std::placeholders::_1, std::placeholders::_2))
{
    initialize();
}

// 先调用 muduo TcpServer::start，再让当前主线程运行 mainLoop_.loop；不是另外创建一个主线程。
void HttpServer::start()
{
    LOG_WARN << "HttpServer[" << server_.name() << "] starts listening on" << server_.ipPort();
    server_.start();
    mainLoop_.loop();
}

// 仅保存连接/消息回调，不立即执行 onConnection/onMessage；绑定 this 不延长当前对象寿命。
void HttpServer::initialize()
{
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this,
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3));
}

// 只有 useSSL_ 为真才初始化 SSL_CTX；失败 abort，调用方必须先安排证书等配置。
void HttpServer::setSslConfig(const ssl::SslConfig& config)
{
    if (useSSL_)
    {
        sslCtx_ = std::make_unique<ssl::SslContext>(config);
        if (!sslCtx_->initialize())
        {
            LOG_ERROR << "Failed to initialize SSL context";
            abort();
        }
    }
}

// 连接建立时在该连接 context 内保存 HttpContext；TLS 分支另建 SSL 包装，断开时删除对应映射。
void HttpServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        if (useSSL_)
        {

            auto sslConn = std::make_unique<ssl::SslConnection>(conn, sslCtx_.get());
            sslConn->setMessageCallback(
                std::bind(&HttpServer::onMessage, this,
                          std::placeholders::_1, std::placeholders::_2,
                          std::placeholders::_3));
            sslConns_[conn] = std::move(sslConn);
            sslConns_[conn]->startHandshake();
        }

        // 每条 TCP 连接独立保存解析状态；后续半包继续使用同一个 context。
        conn->setContext(HttpContext());
    }
    else
    {

        if (useSSL_)
        {
            sslConns_.erase(conn);
        }
    }
}

// 输入是当前连接、其 Buffer 和时间。解析失败发 400；解析不完整保留状态；完整才调用 onRequest。当前每回调仅分发一个请求。
void HttpServer::onMessage(const muduo::net::TcpConnectionPtr &conn,
                           muduo::net::Buffer *buf,
                           muduo::Timestamp receiveTime)
{
    try
    {

        if (useSSL_)
        {
            auto it = sslConns_.find(conn);
            if (it != sslConns_.end())
            {
                it->second->onRead(conn, buf, receiveTime);

                if (!it->second->isHandshakeCompleted())
                {
                    return;
                }

                muduo::net::Buffer* decryptedBuf = it->second->getDecryptedBuffer();
                if (decryptedBuf->readableBytes() == 0)
                    return;
                buf = decryptedBuf;
            }
        }

        // 取出连接级解析器；这里没有检查 any_cast 失败后的空指针。
        HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());
        // false 为解析错误；true 可能仍在等待数据，必须另查 gotAll。
        if (!context->parseRequest(buf, receiveTime))
        {

            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            // 请求关闭写方向的具体时机由 muduo 处理；不能据此认为所有连接资源已立即释放。
            conn->shutdown();
        }

        // 完整才分发。当前不是 while：缓冲区中后续完整请求可能滞留至下一次数据回调。
        if (context->gotAll())
        {
            onRequest(conn, context->request());
            context->reset();
        }

    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Exception in onMessage: " << e.what();
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        // 请求关闭写方向的具体时机由 muduo 处理；不能据此认为所有连接资源已立即释放。
        conn->shutdown();
    }
}

// 根据 Connection/版本计算默认关闭标记，调用处理器填写响应，再序列化发送；Handler 可以覆盖关闭标记。
void HttpServer::onRequest(const muduo::net::TcpConnectionPtr &conn, const HttpRequest &req)
{

    const std::string &connection = req.getHeader("Connection");
    // 当前按字符串精确比较 Connection；HTTP/1.1 默认保持，HTTP/1.0 需显式 Keep-Alive。
    bool close = ((connection == "close") ||
                  (req.getVersion() == "HTTP/1.0" && connection != "Keep-Alive"));
    HttpResponse response(close);

    httpCallback_(req, &response);

    muduo::net::Buffer buf;
    response.appendToBuffer(&buf);
    LOG_INFO << "Sending response:\n" << buf.toStringPiece().as_string();
    // 交给 muduo 的 TCP 发送路径，不代表对端已收完；当前 TLS 路径也未改用 SslConnection::send。
    conn->send(&buf);

    if (response.closeConnection())
    {
        // 请求关闭写方向的具体时机由 muduo 处理；不能据此认为所有连接资源已立即释放。
        conn->shutdown();
    }
}

// 复制请求后执行 before → route → after。抛出的 HttpResponse 直接成为结果；其他标准异常转 500，异常路径跳过剩余步骤。
void HttpServer::handleRequest(const HttpRequest &req, HttpResponse *resp)
{
    try
    {
        HttpRequest mutableReq = req;

        // 前置逻辑在当前线程同步执行；抛响应可短路路由。
        middlewareChain_.processBefore(mutableReq);

        if (!router_.route(mutableReq, resp))
        {
            LOG_INFO << "请求的啥，url：" << req.method() << " " << req.path();
            LOG_INFO << "未找到路由，返回404";
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }

        // 正常分发后逆序执行；异常捕获分支不会回到这里补执行。
        middlewareChain_.processAfter(*resp);
    }
    catch (const HttpResponse& res)
    {

        *resp = res;
    }
    catch (const std::exception& e)
    {

        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setBody(e.what());
    }
}

}
