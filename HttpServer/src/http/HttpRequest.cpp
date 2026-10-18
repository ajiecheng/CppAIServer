// HttpRequest.cpp：结构化请求对象：保存方法、版本、路径、查询参数、路径参数、请求头和原始正文。
// HTTP 解析不提取业务 JSON 字段；用户名、密码等由 Handler 再解析正文。
// 请求头键按原样保存并精确查找；查询参数未做 URL 解码；方法枚举不等于解析器全部支持。

#include "../../include/http/HttpRequest.h"
#include <cassert>

namespace http
{

void HttpRequest::setReceiveTime(muduo::Timestamp t)
{
    receiveTime_ = t;
}

// 识别 GET/POST/PUT/DELETE/OPTIONS，未知方法返回 false；枚举虽有 HEAD，这里未识别。
bool HttpRequest::setMethod(const char *start, const char *end)
{
    assert(method_ == kInvalid);
    std::string m(start, end);

    if (m == "GET")          method_ = kGet;
    else if (m == "POST")    method_ = kPost;
    else if (m == "PUT")     method_ = kPut;
    else if (m == "DELETE")  method_ = kDelete;
    else if (m == "OPTIONS") method_ = kOptions;
    else                     method_ = kInvalid;

    return method_ != kInvalid;
}

// 复制 [start,end) 为路径，不持有 Buffer 中字符的借用指针。
void HttpRequest::setPath(const char *start, const char *end)
{
    path_.assign(start, end);
}

void HttpRequest::setPathParameters(const std::string &key, const std::string &value)
{
    pathParameters_[key] = value;
}

std::string HttpRequest::getPathParameters(const std::string &key) const
{
    auto it = pathParameters_.find(key);
    return (it != pathParameters_.end()) ? it->second : "";
}

std::string HttpRequest::getQueryParameters(const std::string &key) const
{
    auto it = queryParameters_.find(key);
    return (it != queryParameters_.end()) ? it->second : "";
}

// 按 & 分组、按第一个 = 拆键值；忽略无等号项，同名键覆盖，不解码百分号或加号。
void HttpRequest::setQueryParameters(const char *start, const char *end)
{
    std::string argumentStr(start, end);
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;

    while ((pos = argumentStr.find('&', prev)) != std::string::npos)
    {
        std::string pair = argumentStr.substr(prev, pos - prev);
        std::string::size_type equalPos = pair.find('=');

        if (equalPos != std::string::npos)
        {
            std::string key = pair.substr(0, equalPos);
            std::string value = pair.substr(equalPos + 1);
            queryParameters_[key] = value;
        }

        prev = pos + 1;
    }

    std::string lastPair = argumentStr.substr(prev);
    std::string::size_type equalPos = lastPair.find('=');
    if (equalPos != std::string::npos)
    {
        std::string key = lastPair.substr(0, equalPos);
        std::string value = lastPair.substr(equalPos + 1);
        queryParameters_[key] = value;
    }
}

// 键原样拷贝，只裁剪值首尾空白；同名键覆盖，查找区分大小写，不符合完整 HTTP 头规范。
void HttpRequest::addHeader(const char *start, const char *colon, const char *end)
{
    std::string key(start, colon);
    ++colon;

    while (colon < end && isspace(*colon))
    {
        ++colon;
    }
    std::string value(colon, end);

    while (!value.empty() && isspace(value[value.size() - 1]))
    {
        value.resize(value.size() - 1);
    }
    headers_[key] = value;
}

std::string HttpRequest::getHeader(const std::string &field) const
{
    auto it = headers_.find(field);
    return (it != headers_.end()) ? it->second : std::string();
}

// 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
void HttpRequest::swap(HttpRequest &that)
{
    // 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
    std::swap(method_, that.method_);
    // 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
    std::swap(path_, that.path_);
    // 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
    std::swap(pathParameters_, that.pathParameters_);
    // 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
    std::swap(queryParameters_, that.queryParameters_);
    // 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
    std::swap(version_, that.version_);
    // 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
    std::swap(headers_, that.headers_);
    // 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
    std::swap(receiveTime_, that.receiveTime_);
}

}
