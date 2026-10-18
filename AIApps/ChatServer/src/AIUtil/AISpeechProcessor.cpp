// AISpeechProcessor.cpp：百度语音合成适配：构造时同步取 token，synthesize 创建任务并轮询，返回音频 URL。
// 失败常返回空串；每次轮询前 sleep 一秒、最多 60 次，但 curl 未设超时，总耗时不保证 60 秒以内。
// recognize 只有声明，当前源码没有语音识别实现；cuid_ 也未参与合成请求。

#include"../include/AIUtil/AISpeechProcessor.h"

// 向 userp 字符串追加本次 curl 响应片段。
static size_t onWriteData(void* buffer, size_t size, size_t nmemb, void* userp) {
    std::string* str = static_cast<std::string*>(userp);
    str->append((char*)buffer, size * nmemb);
    return size * nmemb;
}

// 同步向 OAuth 端点发送 client_id/secret，解析 token；curl/解析失败返回空串，无 token 刷新机制。
std::string AISpeechProcessor::getAccessToken() {
    std::string result;
    CURL *curl = curl_easy_init();
    if (!curl) return "";

    curl_easy_setopt(curl, CURLOPT_URL, "https://aip.baidubce.com/oauth/2.0/token");
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string data = "grant_type=client_credentials&client_id="
        + client_id_ + "&client_secret=" + client_secret_;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
    if (headers) curl_slist_free_all(headers);
    if (res != CURLE_OK) return "";

    try {
        auto j = json::parse(result);
        if (j.contains("access_token") && j["access_token"].is_string()) {
            return j["access_token"].get<std::string>();
        }
    } catch (...) {}

    return "";
}

// 同步创建 TTS 任务，最多轮询 60 次取得 speech_url；失败/超出次数可返回空串，网络无超时。
std::string AISpeechProcessor::synthesize(const std::string& text,
                                          const std::string& format,
                                          const std::string& lang,
                                          int speed,
                                          int pitch,
                                          int volume)
{
    CURL* curl = nullptr;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (!curl) return "";

    std::string create_url = "https://aip.baidubce.com/rpc/2.0/tts/v1/create?access_token=" + token_;
    curl_easy_setopt(curl, CURLOPT_URL, create_url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    json body = {
        {"text", text},
        {"format", format},
        {"lang", lang},
        {"speed", speed},
        {"pitch", pitch},
        {"volume", volume},
        {"enable_subtitle", 0}
    };

    std::string data = body.dump();
    response.clear();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (headers) curl_slist_free_all(headers);
    if (res != CURLE_OK) return "";

    std::string task_id;
    try {
        json result_json = json::parse(response);
        if (result_json.contains("task_id") && result_json["task_id"].is_string()) {
            task_id = result_json["task_id"].get<std::string>();
        } else if (result_json.contains("tasks_info") && result_json["tasks_info"].is_array()
                   && !result_json["tasks_info"].empty()) {
            task_id = result_json["tasks_info"][0]["task_id"].get<std::string>();
        }
    } catch (...) { return ""; }

    if (task_id.empty()) return "";

    std::string speech_url;
    json query;
    query["task_ids"] = json::array({task_id});

    const int max_loops = 60;
    int loops = 0;
    while (loops++ < max_loops) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        curl = curl_easy_init();
        if (!curl) break;

        std::string query_url = "https://aip.baidubce.com/rpc/2.0/tts/v1/query?access_token=" + token_;
        curl_easy_setopt(curl, CURLOPT_URL, query_url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        data = query.dump();
        response.clear();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWriteData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (headers) curl_slist_free_all(headers);
        if (res != CURLE_OK) break;

        try {
            json queryResult = json::parse(response);
            if (queryResult.contains("tasks_info") && queryResult["tasks_info"].is_array()
                && !queryResult["tasks_info"].empty()) {
                json task = queryResult["tasks_info"][0];
                if (task.contains("task_status") && task["task_status"].is_string()) {
                    std::string status = task["task_status"].get<std::string>();
                    if (status == "Success" && task.contains("task_result")
                        && task["task_result"].contains("speech_url")) {
                        speech_url = task["task_result"]["speech_url"].get<std::string>();
                        break;
                    }
                }
            }
        } catch (...) { break; }
    }

    return speech_url;
}
