// HttpResponse.h：响应数据对象：Handler 填写状态、头和正文，appendToBuffer 再编码为 HTTP 报文字节。
// 输出顺序：状态行、连接头、其他响应头、空行、正文；行结束符为 CRLF。
// setBody 不自动设置 Content-Length，调用者负责字段一致；同名响应头会覆盖，不能表示多个 Set-Cookie。

#pragma once

#include <muduo/net/TcpServer.h>

namespace http
{

class HttpResponse
{
public:

    enum HttpStatusCode
    {
        kUnknown,
        k200Ok = 200,
        k204NoContent = 204,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k401Unauthorized = 401,
        k403Forbidden = 403,
        k404NotFound = 404,
        k409Conflict = 409,
        k500InternalServerError = 500,
    };

    HttpResponse(bool close = true)
        : statusCode_(kUnknown)
        , closeConnection_(close)
    {}

    void setVersion(std::string version) { httpVersion_ = version; }
    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    HttpStatusCode getStatusCode() const { return statusCode_; }
    void setStatusMessage(const std::string message) { statusMessage_ = message; }

    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const { return closeConnection_; }

    void setContentType(const std::string& contentType)
    { addHeader("Content-Type", contentType); }

    void setContentLength(uint64_t length)
    { addHeader("Content-Length", std::to_string(length)); }

    void addHeader(const std::string& key, const std::string& value)
    { headers_[key] = value; }

    void setBody(const std::string& body) { body_ = body; }

    // 同时设置协议版本、状态码和原因短语；缺少这些字段的响应可能生成不完整状态行。
    void setStatusLine(const std::string& version,
                        HttpStatusCode statusCode,
                        const std::string& statusMessage);

    // 按 HTTP 线格式追加状态行、连接头、headers_、空行和 body_；不执行网络发送，也不自动计算正文长度。
    void appendToBuffer(muduo::net::Buffer* outputBuf) const;

private:
    std::string                        httpVersion_;
    HttpStatusCode                     statusCode_;
    std::string                        statusMessage_;
    bool                               closeConnection_;
    std::map<std::string, std::string> headers_;
    std::string                        body_;
};

}
