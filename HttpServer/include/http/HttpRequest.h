// HttpRequest.h：结构化请求对象：保存方法、版本、路径、查询参数、路径参数、请求头和原始正文。
// HTTP 解析不提取业务 JSON 字段；用户名、密码等由 Handler 再解析正文。
// 请求头键按原样保存并精确查找；查询参数未做 URL 解码；方法枚举不等于解析器全部支持。

#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <muduo/base/Timestamp.h>

namespace http
{

class HttpRequest
{
public:

    enum Method
    {
        kInvalid, kGet, kPost, kHead, kPut, kDelete, kOptions
    };

    HttpRequest()
        : method_(kInvalid)
        , version_("Unknown")
    {
    }

    void setReceiveTime(muduo::Timestamp t);
    muduo::Timestamp receiveTime() const { return receiveTime_; }

    // 识别 GET/POST/PUT/DELETE/OPTIONS，未知方法返回 false；枚举虽有 HEAD，这里未识别。
    bool setMethod(const char* start, const char* end);
    Method method() const { return method_; }

    // 复制 [start,end) 为路径，不持有 Buffer 中字符的借用指针。
    void setPath(const char* start, const char* end);
    std::string path() const { return path_; }

    void setPathParameters(const std::string &key, const std::string &value);
    std::string getPathParameters(const std::string &key) const;

    // 按 & 分组、按第一个 = 拆键值；忽略无等号项，同名键覆盖，不解码百分号或加号。
    void setQueryParameters(const char* start, const char* end);
    std::string getQueryParameters(const std::string &key) const;

    void setVersion(std::string v) { version_ = v; }
    std::string getVersion() const { return version_; }

    // 键原样拷贝，只裁剪值首尾空白；同名键覆盖，查找区分大小写，不符合完整 HTTP 头规范。
    void addHeader(const char* start, const char* colon, const char* end);
    std::string getHeader(const std::string& field) const;
    const std::map<std::string, std::string>& headers() const
    { return headers_; }

    void setBody(const std::string& body) { content_ = body; }
    void setBody(const char* start, const char* end)
    {
        if (end >= start)
        {
            content_.assign(start, end - start);
        }
    }
    std::string getBody() const { return content_; }

    void setContentLength(uint64_t length) { contentLength_ = length; }
    uint64_t contentLength() const { return contentLength_; }

    // 交换列出的请求字段；当前未交换 content_、contentLength_，会影响 HttpContext::reset 的完整性。
    void swap(HttpRequest& that);

private:
    Method                                       method_;
    std::string                                  version_;
    std::string                                  path_;
    std::unordered_map<std::string, std::string> pathParameters_;
    std::unordered_map<std::string, std::string> queryParameters_;
    muduo::Timestamp                             receiveTime_;
    std::map<std::string, std::string>           headers_;
    // 原始请求正文；JSON 字段由业务 Handler 解析。
    std::string                                  content_;
    // 声明的正文长度；当前 swap 未交换该字段，重置存在遗漏。
    uint64_t                                     contentLength_ { 0 };
};

}
