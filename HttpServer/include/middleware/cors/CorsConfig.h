// CorsConfig.h：CORS 策略数据：允许的源、方法、请求头、凭据标记和预检缓存秒数。
// 默认允许所有源、列出的五种方法及两种请求头，allowCredentials 默认 false。
// 这些是跨域策略，不是服务端登录认证；配置字段是否实际校验以 CorsMiddleware 为准。

#pragma once

#include <string>
#include <vector>

namespace http
{
namespace middleware
{

struct CorsConfig
{
    std::vector<std::string> allowedOrigins;
    std::vector<std::string> allowedMethods;
    std::vector<std::string> allowedHeaders;
    bool allowCredentials = false;
    int maxAge = 3600;

    static CorsConfig defaultConfig()
    {
        CorsConfig config;
        config.allowedOrigins = {"*"};
        config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        config.allowedHeaders = {"Content-Type", "Authorization"};
        return config;
    }
};

}
}
