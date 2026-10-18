// AIHelper.cpp：单个聊天会话的业务状态：保存消息历史和当前策略，提供同步模型请求与工具推理编排。
// 普通路径：存用户消息 → 策略构造请求 → 同步 curl → 解析回答 → 存回答。
// 工具路径：临时提示词推理 → 可选本地工具调用 → 第二次推理 → 保存原问题与最终答案。
// 历史只有文本和毫秒时间戳，角色由其他代码按奇偶推断；没有内部锁或失败事务回滚。

#include"../include/AIUtil/AIHelper.h"
#include"../include/AIUtil/MQManager.h"
#include <stdexcept>
#include<chrono>

// 默认创建编号 1 策略，会读取密钥；因此创建空历史对象也可能因缺密钥失败。
AIHelper::AIHelper() {
    strategy = StrategyFactory::instance().create("1");
}

// 替换共享策略引用；没有锁，同一 AIHelper 并发切换策略不安全。
void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat) {
    strategy = strat;
}

// 先写内存文本/毫秒时间，再同步发布 SQL 给 MQ；发布失败不会回滚已追加消息。
void AIHelper::addMessage(int userId, const std::string& userName, bool is_user,
                          const std::string& userInput, std::string sessionId) {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    messages.push_back({ userInput, ms });
    pushMessageToMysql(userId, userName, is_user, userInput, ms, sessionId);
}

// 仅恢复文本与原时间，不重复投递写库；不保存 is_user 角色。
void AIHelper::restoreMessage(const std::string& userInput, long long ms) {
    messages.push_back({ userInput, ms });
}

// 根据策略分普通/工具两路；curl 和工具执行同步占用调用线程，临时消息 push/pop 无异常回滚保护。
std::string AIHelper::chat(int userId, std::string userName, std::string sessionId,
                           std::string userQuestion, std::string modelType) {

    setStrategy(StrategyFactory::instance().create(modelType));

    if (false == strategy->isMCPModel) {

        addMessage(userId, userName, true, userQuestion, sessionId);

        json payload = strategy->buildRequest(this->messages);

        json response = executeCurl(payload);

        if (response.contains("error") && response["error"].contains("message")) {
            std::string errMsg = "[API Error] " + response["error"]["message"].get<std::string>();
            addMessage(userId, userName, false, errMsg, sessionId);
            return errMsg;
        }

        std::string answer = strategy->parseResponse(response);

        addMessage(userId, userName, false, answer, sessionId);

        return answer.empty() ? "[Error] 无法解析响应: " + response.dump() : answer;
    }

    AIConfig config;
    config.loadFromFile("../AIApps/ChatServer/resource/config.json");

    std::string tempUserQuestion = config.buildPrompt(userQuestion);
    std::cout << "tempUserQuestion is " << tempUserQuestion << std::endl;

    // 临时提示词加入共享历史，正常路径 pop_back；后续异常可能使其残留。
    messages.push_back({ tempUserQuestion, 0 });

    json firstReq = strategy->buildRequest(this->messages);
    json firstResp = executeCurl(firstReq);
    std::string aiResult = strategy->parseResponse(firstResp);

    messages.pop_back();

    std::cout << "aiResult is " << aiResult << std::endl;

    AIToolCall call = config.parseAIResponse(aiResult);

    if (!call.isToolCall) {
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, aiResult, sessionId);
        std::cout << "No tools required" << std::endl;
        return aiResult;
    }

    json toolResult;
    AIToolRegistry registry;

    try {
        toolResult = registry.invoke(call.toolName, call.args);
        std::cout << "Tool call success" << std::endl;
    }
    catch (const std::exception& e) {
        std::string err = "[工具调用失败] " + std::string(e.what());
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, err, sessionId);
        std::cout << "Tool call failed" << std::endl << std::string(e.what());
        return err;
    }

    std::string secondPrompt = config.buildToolResultPrompt(
        userQuestion, call.toolName, call.args, toolResult);

    std::cout << "secondPrompt is " << secondPrompt << std::endl;
    messages.push_back({ secondPrompt, 0 });

    json secondReq = strategy->buildRequest(messages);
    json secondResp = executeCurl(secondReq);
    std::string finalAnswer = strategy->parseResponse(secondResp);

    messages.pop_back();

    std::cout << "finalAnswer is " << finalAnswer << std::endl;

    addMessage(userId, userName, true, userQuestion, sessionId);
    addMessage(userId, userName, false, finalAnswer, sessionId);
    return finalAnswer;
}

// 使用当前策略端点同步提交调用方 JSON，不自动记录聊天历史。
json AIHelper::request(const json& payload) {
    return executeCurl(payload);
}

// 按值复制历史；返回副本不代表复制过程中有并发保护。
std::vector<std::pair<std::string, long long>> AIHelper::GetMessages() {
    return this->messages;
}

// 构造认证头及 JSON POST，同步收完整响应后解析；无超时/HTTP状态校验，当前调试输出还包含密钥。
json AIHelper::executeCurl(const json& payload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize curl");
    }

    std::cout << "test " << strategy->getApiUrl().c_str() << ' ' << strategy->getApiKey() << std::endl;

    std::string readBuffer;
    struct curl_slist* headers = nullptr;

    std::string authHeader = "Authorization: Bearer " + strategy->getApiKey();
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string payloadStr = payload.dump();

    curl_easy_setopt(curl, CURLOPT_URL, strategy->getApiUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    // 同步网络调用：若从 IO 线程进入，会阻塞该线程处理其他连接。
    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
    }

    try {
        return json::parse(readBuffer);
    }
    catch (...) {
        throw std::runtime_error("Failed to parse JSON response: " + readBuffer);
    }
}

// curl 多次回调，每次向 userp 指向的字符串追加 size*nmemb 字节，并返回已接收长度。
size_t AIHelper::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// 手工转义部分 SQL 字符；依赖数据库转义模式，不能等同于参数化查询或完整注入防护。
std::string AIHelper::escapeString(const std::string& input) {
    std::string output;
    output.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '\'': output += "\\\'"; break;
            case '\"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:   output += c; break;
        }
    }
    return output;
}

// 拼 INSERT 后同步发布至 sql_queue，消费者再执行；sessionId 未加引号/转义，入库失败可能晚于 HTTP 成功。
void AIHelper::pushMessageToMysql(int userId, const std::string& userName, bool is_user,
                                  const std::string& userInput, long long ms, std::string sessionId) {
    std::string safeUserName = escapeString(userName);
    std::string safeUserInput = escapeString(userInput);

    std::string sql = "INSERT INTO chat_message (id, username, session_id, is_user, content, ts) VALUES ("
        + std::to_string(userId) + ", "
        + "'" + safeUserName + "', "
        + sessionId + ", "
        + std::to_string(is_user ? 1 : 0) + ", "
        + "'" + safeUserInput + "', "
        + std::to_string(ms) + ")";

    // 只表示尝试投递，不是数据库提交确认；内存、队列、数据库没有原子事务。
    MQManager::instance().publish("sql_queue", sql);
}
