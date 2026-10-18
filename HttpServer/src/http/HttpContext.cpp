// HttpContext.cpp：连接级 HTTP 解析状态：保存 state_ 和正在组装的 request_，跨多次数据到达继续解析。
// 请求行 → 请求头 → 按 Content-Length 读取正文 → kGotAll；完整后由 HttpServer 分发并 reset。
// parseRequest 返回 false 表示格式错误；返回 true 还需 gotAll() 才能判断请求完整。
// 只实现当前 HTTP/1.0、1.1 子集，不支持 chunked；一次调用完成一个请求，不自动循环分发后续请求。

#include "../../include/http/HttpContext.h"

using namespace muduo;
using namespace muduo::net;

namespace http
{

// 推进当前请求的状态机。返回 true 包含“尚未收全”；只消费已确定的部分，调用者用 gotAll 判断完成。
bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime)
{
    bool ok = true;
    bool hasMore = true;
    while (hasMore)
    {

        // 阶段 1：仅在找到完整 CRLF 行后解析，成功再消费请求行。
        if (state_ == kExpectRequestLine)
        {

            const char *crlf = buf->findCRLF();
            if (crlf)
            {

                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    request_.setReceiveTime(receiveTime);

                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }

        // 阶段 2：逐行保存头；空行结束头部，当前仅为 POST/PUT 按长度读正文。
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon < crlf)
                {

                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else if (buf->peek() == crlf)
                {

                    if (request_.method() == HttpRequest::kPost ||
                        request_.method() == HttpRequest::kPut)
                    {
                        std::string contentLength = request_.getHeader("Content-Length");
                        if (!contentLength.empty())
                        {
                            // stoi 可抛异常，且未严格拒绝负数/尾随字符或限制正文大小。
                            request_.setContentLength(std::stoi(contentLength));
                            if (request_.contentLength() > 0)
                            {
                                state_ = kExpectBody;
                            }
                            else
                            {
                                state_ = kGotAll;
                                hasMore = false;
                            }
                        }
                        else
                        {

                            ok = false;
                            hasMore = false;
                        }
                    }
                    else
                    {

                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                else
                {

                    ok = false;
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }

        // 阶段 3：正文按字节计数，不查找换行，也不解析业务 JSON。
        else if (state_ == kExpectBody)
        {

            // 半包：不消费当前正文，保留数据与 kExpectBody，等待下次字节到达。
            if (buf->readableBytes() < request_.contentLength())
            {
                hasMore = false;
                return true;
            }

            // 只复制本请求声明的正文长度；后续字节留在接收 Buffer。
            std::string body(buf->peek(), buf->peek() + request_.contentLength());
            request_.setBody(body);
            // 推进读位置，不应把整个 Buffer 清空，否则会丢弃后续请求字节。
            buf->retrieve(request_.contentLength());
            state_ = kGotAll;
            hasMore = false;
        }
    }
    return ok;
}

// 在 [begin,end) 按空格拆方法、目标、版本；路径与查询分开保存，仅接受 HTTP/1.0 或 1.1。
bool HttpContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;

    const char *space = std::find(start, end, ' ');
    if (space != end && request_.setMethod(start, space))
    {
        start = space + 1;

        space = std::find(start, end, ' ');
        if (space != end)
        {
            const char *argumentStart = std::find(start, space, '?');
            if (argumentStart != space)
            {

                request_.setPath(start, argumentStart);
                request_.setQueryParameters(argumentStart + 1, space);
            }
            else
            {

                request_.setPath(start, space);
            }

            start = space + 1;

            succeed = ((end - start == 8) && std::equal(start, end - 1, "HTTP/1."));
            if (succeed)
            {
                if (*(end - 1) == '1')
                {
                    request_.setVersion("HTTP/1.1");
                }
                else if (*(end - 1) == '0')
                {
                    request_.setVersion("HTTP/1.0");
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

}
