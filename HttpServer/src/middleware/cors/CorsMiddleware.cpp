// CorsMiddleware.cpp：浏览器跨域支持：OPTIONS 在 before 中生成响应并抛出，普通响应在 after 中添加 CORS 头。
// 预检检查 Origin；当前未逐项校验预检方法/请求头，after 对普通请求也未匹配其 Origin。
// after 使用通配符或配置的第一个源；不能据此宣称支持任意凭据跨域请求。

#include "../../../include/middleware/cors/CorsMiddleware.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <muduo/base/Logging.h>

namespace http
{
namespace middleware
{

CorsMiddleware::CorsMiddleware(const CorsConfig& config) : config_(config) {}

// 当前把所有 OPTIONS 都交给预检处理，抛 HttpResponse 使 HttpServer 跳过路由。
void CorsMiddleware::before(HttpRequest& request)
{
    if (request.method() == HttpRequest::Method::kOptions)
    {
        LOG_INFO << "Processing CORS preflight request";
        HttpResponse response;
        handlePreflightRequest(request, response);
        throw response;
    }
}

// 根据配置选通配符或第一个源写头，没有当前请求 Origin 参数，不能按请求源动态匹配。
void CorsMiddleware::after(HttpResponse& response)
{
    if (!config_.allowedOrigins.empty())
    {

        if (std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*")
            != config_.allowedOrigins.end())
        {
            addCorsHeaders(response, "*");
        }
        else
        {

            addCorsHeaders(response, config_.allowedOrigins[0]);
        }
    }
}

// 空源列表或含通配符时允许任意源，否则精确比较字符串。
bool CorsMiddleware::isOriginAllowed(const std::string& origin) const
{
    return config_.allowedOrigins.empty() ||
           std::find(config_.allowedOrigins.begin(),
                    config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end() ||
           std::find(config_.allowedOrigins.begin(),
                    config_.allowedOrigins.end(), origin) != config_.allowedOrigins.end();
}

// 读取 Origin，拒绝时设 403；允许时添加 CORS 头并设 204，未设置状态行版本和原因短语。
void CorsMiddleware::handlePreflightRequest(const HttpRequest& request,
                                          HttpResponse& response)
{
    const std::string& origin = request.getHeader("Origin");

    if (!isOriginAllowed(origin))
    {
        LOG_WARN << "Origin not allowed: " << origin;
        response.setStatusCode(HttpResponse::k403Forbidden);
        return;
    }

    addCorsHeaders(response, origin);
    response.setStatusCode(HttpResponse::k204NoContent);
    LOG_INFO << "Preflight request processed successfully";
}

// 写入允许源、可选凭据标记、方法/头列表及 max-age；这些头不能代替业务身份校验。
void CorsMiddleware::addCorsHeaders(HttpResponse& response,
                                  const std::string& origin)
{
    try
    {
        response.addHeader("Access-Control-Allow-Origin", origin);

        if (config_.allowCredentials)
        {
            response.addHeader("Access-Control-Allow-Credentials", "true");
        }

        if (!config_.allowedMethods.empty())
        {
            response.addHeader("Access-Control-Allow-Methods",
                             join(config_.allowedMethods, ", "));
        }

        if (!config_.allowedHeaders.empty())
        {
            response.addHeader("Access-Control-Allow-Headers",
                             join(config_.allowedHeaders, ", "));
        }

        response.addHeader("Access-Control-Max-Age",
                          std::to_string(config_.maxAge));

        LOG_DEBUG << "CORS headers added successfully";
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Error adding CORS headers: " << e.what();
    }
}

// 按分隔符连接列表，首项前不加分隔符。
std::string CorsMiddleware::join(const std::vector<std::string>& strings, const std::string& delimiter)
{
    std::ostringstream result;
    for (size_t i = 0; i < strings.size(); ++i)
    {
        if (i > 0) result << delimiter;
        result << strings[i];
    }
    return result.str();
}

}
}
