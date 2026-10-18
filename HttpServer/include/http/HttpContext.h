// HttpContext.h：连接级 HTTP 解析状态：保存 state_ 和正在组装的 request_，跨多次数据到达继续解析。
// 请求行 → 请求头 → 按 Content-Length 读取正文 → kGotAll；完整后由 HttpServer 分发并 reset。
// parseRequest 返回 false 表示格式错误；返回 true 还需 gotAll() 才能判断请求完整。
// 只实现当前 HTTP/1.0、1.1 子集，不支持 chunked；一次调用完成一个请求，不自动循环分发后续请求。

#pragma once

#include <iostream>
#include <muduo/net/TcpServer.h>
#include "HttpRequest.h"

namespace http
{

class HttpContext
{
public:

    enum HttpRequestParseState
    {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    HttpContext()
    : state_(kExpectRequestLine)
    {}

    // 推进当前请求的状态机。返回 true 包含“尚未收全”；只消费已确定的部分，调用者用 gotAll 判断完成。
    bool parseRequest(muduo::net::Buffer* buf, muduo::Timestamp receiveTime);

    bool gotAll() const
    { return state_ == kGotAll; }

    // 回到请求行状态并与默认请求交换。注意 HttpRequest::swap 当前遗漏正文和长度，reset 未完全清空这两个字段。
    void reset()
    {
        state_ = kExpectRequestLine;
        HttpRequest dummyData;
        request_.swap(dummyData);
    }

    const HttpRequest& request() const
    { return request_; }

    HttpRequest& request()
    { return request_; }

private:

    // 在 [begin,end) 按空格拆方法、目标、版本；路径与查询分开保存，仅接受 HTTP/1.0 或 1.1。
    bool processRequestLine(const char* begin, const char* end);

private:
    // 跨回调保留解析阶段，不应每次收数据重新创建。
    HttpRequestParseState state_;
    // 拥有已解析字段及正文副本；Buffer 则拥有尚未消费的输入字节。
    HttpRequest           request_;
};

}
