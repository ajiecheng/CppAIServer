// CorsMiddleware.h：浏览器跨域支持：OPTIONS 在 before 中生成响应并抛出，普通响应在 after 中添加 CORS 头。
// 预检检查 Origin；当前未逐项校验预检方法/请求头，after 对普通请求也未匹配其 Origin。
// after 使用通配符或配置的第一个源；不能据此宣称支持任意凭据跨域请求。

#pragma once

#include "../Middleware.h"
#include "../../http/HttpRequest.h"
#include "../../http/HttpResponse.h"
#include "CorsConfig.h"

namespace http
{
namespace middleware
{

class CorsMiddleware : public Middleware
{
public:
    explicit CorsMiddleware(const CorsConfig& config = CorsConfig::defaultConfig());

    // 当前把所有 OPTIONS 都交给预检处理，抛 HttpResponse 使 HttpServer 跳过路由。
    void before(HttpRequest& request) override;
    // 根据配置选通配符或第一个源写头，没有当前请求 Origin 参数，不能按请求源动态匹配。
    void after(HttpResponse& response) override;

    // 按分隔符连接列表，首项前不加分隔符。
    std::string join(const std::vector<std::string>& strings, const std::string& delimiter);

private:
    // 空源列表或含通配符时允许任意源，否则精确比较字符串。
    bool isOriginAllowed(const std::string& origin) const;
    // 读取 Origin，拒绝时设 403；允许时添加 CORS 头并设 204，未设置状态行版本和原因短语。
    void handlePreflightRequest(const HttpRequest& request, HttpResponse& response);
    // 写入允许源、可选凭据标记、方法/头列表及 max-age；这些头不能代替业务身份校验。
    void addCorsHeaders(HttpResponse& response, const std::string& origin);

private:
    CorsConfig config_;
};

}
}
