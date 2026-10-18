// MQManager.h：数据库异步写入的消息通路：MQManager 发布 SQL，RabbitMQThreadPool 消费并调用传入 handler。
// 生产者原子计数轮询 Channel，每个 Channel 有独立锁；发布操作本身仍是同步调用。
// 消费者 handler 正常返回后才 ACK；发生异常会退出该 worker，没有自动重启、幂等或无限重试保证。
// 队列声明参数与消息属性须结合 SimpleAmqpClient 版本核实，不能把当前代码描述为绝不丢失或恰好一次。

#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <functional>

class MQManager {
public:
    static MQManager& instance() {
        static MQManager mgr;
        return mgr;
    }

    void publish(const std::string& queue, const std::string& msg);

private:

    struct MQConn {
        AmqpClient::Channel::ptr_t channel;
        std::mutex mtx;
    };

    MQManager(size_t poolSize = 5);

    MQManager(const MQManager&) = delete;
    MQManager& operator=(const MQManager&) = delete;

    std::vector<std::shared_ptr<MQConn>> pool_;
    size_t poolSize_;
    std::atomic<size_t> counter_;
};

class RabbitMQThreadPool {
public:
    using HandlerFunc = std::function<void(const std::string&)>;

    RabbitMQThreadPool(const std::string& host,
        const std::string& queue,
        int thread_num,
        HandlerFunc handler)
        : stop_(false),
        rabbitmq_host_(host),
        queue_name_(queue),
        thread_num_(thread_num),
        handler_(handler) {}

    void start();
    void shutdown();

    ~RabbitMQThreadPool() {
        shutdown();
    }

private:

    void worker(int id);

    std::vector<std::thread> workers_;
    std::atomic<bool> stop_;
    std::string queue_name_;
    int thread_num_;
    std::string rabbitmq_host_;
    HandlerFunc handler_;
};
