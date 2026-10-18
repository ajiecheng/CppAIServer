// AISessionIdGenerator.h：生成聊天会话编号：系统时钟计数与 rand 值异或后转十进制字符串。
// 此 ID 不等于 Cookie 登录会话 ID；构造时反复 srand 会影响全局随机序列，不能保证唯一或安全。

#pragma once
#include <chrono>
#include <random>
#include <cstdlib>
#include <ctime>
#include <string>

class AISessionIdGenerator {
public:
    AISessionIdGenerator() {

        std::srand(static_cast<unsigned>(std::time(nullptr)));
    }

    // 把时钟计数与有限范围 rand 异或并转字符串；没有碰撞检查，不适合作安全凭证。
    std::string generate();
};
