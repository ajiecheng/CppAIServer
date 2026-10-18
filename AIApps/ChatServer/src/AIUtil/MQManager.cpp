// MQManager.cpp：数据库异步写入的消息通路：MQManager 发布 SQL，RabbitMQThreadPool 消费并调用传入 handler。
// 生产者原子计数轮询 Channel，每个 Channel 有独立锁；发布操作本身仍是同步调用。
// 消费者 handler 正常返回后才 ACK；发生异常会退出该 worker，没有自动重启、幂等或无限重试保证。
// 队列声明参数与消息属性须结合 SimpleAmqpClient 版本核实，不能把当前代码描述为绝不丢失或恰好一次。

#include"../include/AIUtil/MQManager.h"

MQManager::MQManager(size_t poolSize)
    : poolSize_(poolSize), counter_(0) {
    for (size_t i = 0; i < poolSize_; ++i) {
        auto conn = std::make_shared<MQConn>();

        conn->channel = AmqpClient::Channel::Create("localhost", 5672, "guest", "guest", "/");
        pool_.push_back(conn);
    }
}

void MQManager::publish(const std::string& queue, const std::string& msg) {
    // 原子计数用于轮询选 Channel；不能推断整条发布路径无锁或无阻塞。
    size_t index = counter_.fetch_add(1) % poolSize_;
    auto& conn = pool_[index];

    std::lock_guard<std::mutex> lock(conn->mtx);
    auto message = AmqpClient::BasicMessage::Create(msg);
    conn->channel->BasicPublish("", queue, message);
}

void RabbitMQThreadPool::start() {
    for (int i = 0; i < thread_num_; ++i) {
        workers_.emplace_back(&RabbitMQThreadPool::worker, this, i);
    }
}

void RabbitMQThreadPool::shutdown() {
    stop_ = true;
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void RabbitMQThreadPool::worker(int id) {
    try {

        auto channel = AmqpClient::Channel::Create(rabbitmq_host_, 5672, "guest", "guest", "/");

        // 布尔实参的准确含义取决于所用库签名；需核实版本，不据此承诺持久化。
        channel->DeclareQueue(queue_name_, false, true, false, false);

        std::string consumer_tag = channel->BasicConsume(queue_name_, "", true, false, false);

        channel->BasicQos(consumer_tag, 1);

        while (!stop_) {
            AmqpClient::Envelope::ptr_t env;

            bool ok = channel->BasicConsumeMessage(consumer_tag, env, 500);
            if (ok && env) {
                std::string msg = env->Message()->Body();
                // 同步执行消费回调；抛异常会跳到 worker 外层 catch，终止这个 worker。
                handler_(msg);
                // 只在回调正常返回后确认；数据库提交与 ACK 间若故障仍可能重复处理。
                channel->BasicAck(env);
            }
        }

        channel->BasicCancel(consumer_tag);
    }
    catch (const std::exception& e) {
        std::cerr << "Thread " << id << " exception: " << e.what() << std::endl;
    }
}
