// HttpServer.h：HTTP 框架与 muduo 的适配层：注册连接/消息回调，组织解析、分发、响应发送。
// 普通 HTTP 主链：onConnection 配置连接级 HttpContext → onMessage → onRequest → handleRequest。
// handleRequest 按前置中间件、路由、后置中间件运行；Session 由具体 Handler 按需访问。
// TLS 是条件分支，当前接入存在缺口；本文件不代表 HTTPS 已完成联调。

#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "../router/Router.h"
#include "../session/SessionManager.h"
#include "../middleware/MiddlewareChain.h"
#include "../middleware/cors/CorsMiddleware.h"
#include "../ssl/SslConnection.h"
#include "../ssl/SslContext.h"

class HttpRequest;
class HttpResponse;

namespace http
{

class HttpServer : muduo::noncopyable
{
public:

    using HttpCallback = std::function<void (const http::HttpRequest&, http::HttpResponse*)>;

    // 配置监听地址、TCP 服务与请求回调。注意成员按头文件声明顺序构造：server_ 早于 mainLoop_，存在依赖顺序风险。
    HttpServer(int port,
               const std::string& name,
               bool useSSL = false,
               muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);

    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    // 先调用 muduo TcpServer::start，再让当前主线程运行 mainLoop_.loop；不是另外创建一个主线程。
    void start();

    muduo::net::EventLoop* getLoop() const
    {
        return server_.getLoop();
    }

    void setHttpCallback(const HttpCallback& cb)
    {
        httpCallback_ = cb;
    }

    void Get(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallback(HttpRequest::kGet, path, cb);
    }

    void Get(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kGet, path, handler);
    }

    void Post(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallback(HttpRequest::kPost, path, cb);
    }

    void Post(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kPost, path, handler);
    }

    void addRoute(HttpRequest::Method method, const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.addRegexHandler(method, path, handler);
    }

    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerCallback& callback)
    {
        router_.addRegexCallback(method, path, callback);
    }

    // 独占登录会话管理器；Get/Post 分发本身不会自动查询 Session。
    void setSessionManager(std::unique_ptr<session::SessionManager> manager)
    {
        sessionManager_ = std::move(manager);
    }
    session::SessionManager* getSessionManager() const
    {
        return sessionManager_.get();
    }

    void addMiddleware(std::shared_ptr<middleware::Middleware> middleware)
    {
        middlewareChain_.addMiddleware(middleware);
    }

    void enableSSL(bool enable)
    {
        useSSL_ = enable;
    }
    // 只有 useSSL_ 为真才初始化 SSL_CTX；失败 abort，调用方必须先安排证书等配置。
    void setSslConfig(const ssl::SslConfig& config);

private:

    // 仅保存连接/消息回调，不立即执行 onConnection/onMessage；绑定 this 不延长当前对象寿命。
    void initialize();

    // 连接建立时在该连接 context 内保存 HttpContext；TLS 分支另建 SSL 包装，断开时删除对应映射。
    void onConnection(const muduo::net::TcpConnectionPtr& conn);

    // 输入是当前连接、其 Buffer 和时间。解析失败发 400；解析不完整保留状态；完整才调用 onRequest。当前每回调仅分发一个请求。
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp receiveTime);

    // 根据 Connection/版本计算默认关闭标记，调用处理器填写响应，再序列化发送；Handler 可以覆盖关闭标记。
    void onRequest(const muduo::net::TcpConnectionPtr&, const HttpRequest&);

    // 复制请求后执行 before → route → after。抛出的 HttpResponse 直接成为结果；其他标准异常转 500，异常路径跳过剩余步骤。
    void handleRequest(const HttpRequest& req, HttpResponse* resp);

private:

    muduo::net::InetAddress  listenAddr_;
    muduo::net::TcpServer    server_;
    // 注意声明顺序：当前 mainLoop_ 晚于 server_ 构造，虽然 server_ 构造已接收其地址；应先构造依赖对象。
    muduo::net::EventLoop    mainLoop_;

    HttpCallback             httpCallback_;

    router::Router           router_;

    // 独占登录会话管理器；Get/Post 分发本身不会自动查询 Session。
    std::unique_ptr<session::SessionManager> sessionManager_;

    middleware::MiddlewareChain middlewareChain_;

    std::unique_ptr<ssl::SslContext> sslCtx_;
    // 是否走 TLS 分支；设置为 true 还不等于证书和加密收发链已经正确接通。
    bool                                useSSL_;

    std::map<muduo::net::TcpConnectionPtr, std::unique_ptr<ssl::SslConnection>> sslConns_;
};

}
