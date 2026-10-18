// ChatEntryHandler.cpp：GET / 或 /entry：读取登录/注册入口 HTML，设置响应状态、长度和正文。
// 无 Session 鉴权；回退页面内容不自动变成 HTTP 404。

#include "../include/handlers/ChatEntryHandler.h"

// 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
void ChatEntryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{

    std::string reqFile;
    reqFile.append("AIApps/ChatServer/resource/entry.html");
    FileUtil fileOperater(reqFile);
    if (!fileOperater.isValid())
    {
        LOG_WARN << reqFile << " not exist";
        fileOperater.resetDefaultFile();
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string bufStr = std::string(buffer.data(), buffer.size());

    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("text/html");
    resp->setContentLength(bufStr.size());
    resp->setBody(bufStr);
}
