// AIFactory.h：策略工厂：模型编号映射到创建函数，create 每次产生策略对象，未知编号抛异常。
// StrategyRegister 静态对象注册 1/2/3/4；局部静态单例初始化安全不等于注册表并发修改安全。

#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>

#include"AIStrategy.h"

class StrategyFactory {
public:
    using Creator = std::function<std::shared_ptr<AIStrategy>()>;

    // 返回局部静态单例；C++11 保证首次初始化同步，不自动保护 creators 的后续并发修改。
    static StrategyFactory& instance();

    // 保存或覆盖创建函数，通常由启动前的静态注册对象调用。
    void registerStrategy(const std::string& name, Creator creator);

    // 按编号查创建函数并调用；未注册抛 runtime_error，返回新策略的 shared_ptr。
    std::shared_ptr<AIStrategy> create(const std::string& name);

private:
    StrategyFactory() = default;
    std::unordered_map<std::string, Creator> creators;
};

template<typename T>
struct StrategyRegister {
    StrategyRegister(const std::string& name) {
        // 返回局部静态单例；C++11 保证首次初始化同步，不自动保护 creators 的后续并发修改。
        StrategyFactory::instance().registerStrategy(name, [] {
            return std::make_shared<T>();
        });
    }
};
