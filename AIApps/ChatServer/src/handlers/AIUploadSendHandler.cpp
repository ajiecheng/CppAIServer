// AIUploadSendHandler.cpp：POST /upload/send：校验登录，读取 JSON 中的 filename/image，Base64 解码后本地分类。
// 按用户复用 ImageRecognizer；映射锁只保护查找/创建，不包住推理；confidence 固定 0.95，不是模型概率。

#include "../include/handlers/AIUploadSendHandler.h"

// 由 Router 同步调用；输入结构化请求，输出写入 resp。具体流程与限制见文件开头及分支注释。
void AIUploadSendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {

        // 从 Cookie 查找或创建服务端会话；获取会话本身不代表已通过登录校验。
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true")
        {
            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);
            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                "Unauthorized", true, "application/json", errorBody.size(), errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));

        std::shared_ptr<ImageRecognizer> ImageRecognizerPtr;
        {
            std::lock_guard<std::mutex> lock(server_->mutexForImageRecognizerMap);
            if (server_->ImageRecognizerMap.find(userId) == server_->ImageRecognizerMap.end()) {

                server_->ImageRecognizerMap.emplace(
                    userId,
                    std::make_shared<ImageRecognizer>(
                        "AIApps/ChatServer/resource/mobilenetv2-7.onnx",
                        "AIApps/ChatServer/resource/imagenet_classes.txt")
                );
            }
            ImageRecognizerPtr = server_->ImageRecognizerMap[userId];
        }

        auto body = req.getBody();
        std::string filename;
        std::string imageBase64;
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("filename")) filename = j["filename"];
            if (j.contains("image")) imageBase64 = j["image"];
        }
        if (imageBase64.empty())
        {
            throw std::runtime_error("No image data provided");
        }

        std::string decodedData = base64_decode(imageBase64);
        std::vector<uchar> imgData(decodedData.begin(), decodedData.end());

        std::string className = ImageRecognizerPtr->PredictFromBuffer(imgData);

        json successResp;
        successResp["success"] = "ok";
        successResp["filename"] = filename;
        successResp["class_name"] = className;
        // 固定展示值，不是 ONNX 输出的概率。
        successResp["confidence"] = 0.95;

        // 业务 JSON 编码为正文；完整 HTTP 报文稍后由 HttpResponse::appendToBuffer 编码。
        std::string successBody = successResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
    }
    catch (const std::exception& e)
    {
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}
