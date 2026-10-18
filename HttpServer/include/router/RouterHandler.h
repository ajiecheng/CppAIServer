// RouterHandler.h：业务处理器接口：Router 调用 handle(req, resp)，具体类读取请求并填写响应。
// req 是只读输入，resp 是由上层持有的可写输出；虚析构支持经基类指针释放派生对象。

#pragma once
#include <string>
#include <memory>
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace router
{

class RouterHandler
{
public:
    virtual ~RouterHandler() = default;

    virtual void handle(const HttpRequest& req, HttpResponse* resp) = 0;
};

}
}
