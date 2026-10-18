// AISpeechProcessor.h：百度语音合成适配：构造时同步取 token，synthesize 创建任务并轮询，返回音频 URL。
// 失败常返回空串；每次轮询前 sleep 一秒、最多 60 次，但 curl 未设超时，总耗时不保证 60 秒以内。
// recognize 只有声明，当前源码没有语音识别实现；cuid_ 也未参与合成请求。

#pragma once
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>
#include <chrono>

#include "../../../../HttpServer/include/utils/JsonUtil.h"
#include"base64.h"

class AISpeechProcessor {
public:
    AISpeechProcessor(const std::string& clientId,
                      const std::string& clientSecret,
                      const std::string& cuid = "RZjSQGzNaA8EFWf6rvuHEKDh9i4XJIV9")
        : client_id_(clientId), client_secret_(clientSecret), cuid_(cuid)
    {
        // 构造立即进行同步网络请求，不是惰性加载或缓存池。
        token_ = getAccessToken();
    }

    // 仅声明：当前 cpp 无定义，未实现语音识别链路。
    std::string recognize(const std::string& speechData,
                          const std::string& format = "pcm",
                          int rate = 16000, int channel = 1);

    // 同步创建 TTS 任务，最多轮询 60 次取得 speech_url；失败/超出次数可返回空串，网络无超时。
    std::string synthesize(const std::string& text,
                           const std::string& format = "mp3-16k",
                           const std::string& lang = "zh",
                           int speed = 5, int pitch = 5, int volume = 5);

private:
    std::string client_id_;
    std::string client_secret_;
    std::string cuid_;
    std::string token_;

    // 同步向 OAuth 端点发送 client_id/secret，解析 token；curl/解析失败返回空串，无 token 刷新机制。
    std::string getAccessToken();
};
