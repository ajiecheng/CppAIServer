// AIHelper.h：单个聊天会话的业务状态：保存消息历史和当前策略，提供同步模型请求与工具推理编排。
// 普通路径：存用户消息 → 策略构造请求 → 同步 curl → 解析回答 → 存回答。
// 工具路径：临时提示词推理 → 可选本地工具调用 → 第二次推理 → 保存原问题与最终答案。
// 历史只有文本和毫秒时间戳，角色由其他代码按奇偶推断；没有内部锁或失败事务回滚。

#pragma once
#include <string>
#include <vector>
#include <utility>
#include <curl/curl.h>
#include <iostream>
#include <sstream>

#include "../../../../HttpServer/include/utils/JsonUtil.h"
#include"../../../../HttpServer/include/utils/MysqlUtil.h"

#include"AIFactory.h"
#include"AIConfig.h"
#include"AIToolRegistry.h"

class AIHelper {
public:
    // 默认创建编号 1 策略，会读取密钥；因此创建空历史对象也可能因缺密钥失败。
    AIHelper();

    // 替换共享策略引用；没有锁，同一 AIHelper 并发切换策略不安全。
    void setStrategy(std::shared_ptr<AIStrategy> strat);

    // 先写内存文本/毫秒时间，再同步发布 SQL 给 MQ；发布失败不会回滚已追加消息。
    void addMessage(int userId, const std::string& userName, bool is_user,
                    const std::string& userInput, std::string sessionId);
    // 仅恢复文本与原时间，不重复投递写库；不保存 is_user 角色。
    void restoreMessage(const std::string& userInput, long long ms);

    // 根据策略分普通/工具两路；curl 和工具执行同步占用调用线程，临时消息 push/pop 无异常回滚保护。
    std::string chat(int userId, std::string userName, std::string sessionId,
                     std::string userQuestion, std::string modelType);

    // 使用当前策略端点同步提交调用方 JSON，不自动记录聊天历史。
    json request(const json& payload);

    // 按值复制历史；返回副本不代表复制过程中有并发保护。
    std::vector<std::pair<std::string, long long>> GetMessages();

private:

    // 手工转义部分 SQL 字符；依赖数据库转义模式，不能等同于参数化查询或完整注入防护。
    std::string escapeString(const std::string& input);
    // 拼 INSERT 后同步发布至 sql_queue，消费者再执行；sessionId 未加引号/转义，入库失败可能晚于 HTTP 成功。
    void pushMessageToMysql(int userId, const std::string& userName, bool is_user,
                            const std::string& userInput, long long ms, std::string sessionId);

    // 构造认证头及 JSON POST，同步收完整响应后解析；无超时/HTTP状态校验，当前调试输出还包含密钥。
    json executeCurl(const json& payload);
    // curl 多次回调，每次向 userp 指向的字符串追加 size*nmemb 字节，并返回已接收长度。
    static size_t WriteCallback(void* contents, size_t size,
                                size_t nmemb, void* userp);

private:

    std::shared_ptr<AIStrategy> strategy;

    // 消息文本与毫秒时间戳；不含显式角色，未加锁。
    std::vector<std::pair<std::string, long long>> messages;
};
