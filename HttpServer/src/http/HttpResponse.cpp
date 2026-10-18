// HttpResponse.cpp：响应数据对象：Handler 填写状态、头和正文，appendToBuffer 再编码为 HTTP 报文字节。
// 输出顺序：状态行、连接头、其他响应头、空行、正文；行结束符为 CRLF。
// setBody 不自动设置 Content-Length，调用者负责字段一致；同名响应头会覆盖，不能表示多个 Set-Cookie。

#include "../../include/http/HttpResponse.h"

namespace http
{

// 按 HTTP 线格式追加状态行、连接头、headers_、空行和 body_；不执行网络发送，也不自动计算正文长度。
void HttpResponse::appendToBuffer(muduo::net::Buffer* outputBuf) const
{
    char buf[32];

    snprintf(buf, sizeof buf, "%s %d ", httpVersion_.c_str(), statusCode_);
    outputBuf->append(buf);
    outputBuf->append(statusMessage_);
    outputBuf->append("\r\n");

    if (closeConnection_)
    {

        outputBuf->append("Connection: close\r\n");
    }
    else
    {

        outputBuf->append("Connection: Keep-Alive\r\n");
    }

    for (const auto& header : headers_)
    {
        outputBuf->append(header.first);
        outputBuf->append(": ");
        outputBuf->append(header.second);
        outputBuf->append("\r\n");
    }

    outputBuf->append("\r\n");

    outputBuf->append(body_);
}

// 同时设置协议版本、状态码和原因短语；缺少这些字段的响应可能生成不完整状态行。
void HttpResponse::setStatusLine(const std::string& version,
                                 HttpStatusCode statusCode,
                                 const std::string& statusMessage)
{
    httpVersion_ = version;
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}

}
