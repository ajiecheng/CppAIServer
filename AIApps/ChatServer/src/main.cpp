// main.cpp：进程入口：解析端口、装配业务服务、恢复历史、启动 MQ 消费线程，再进入网络事件循环。
// 阅读顺序：main → ChatServer 构造/initialize → HttpServer::start；请求到达后改走网络回调。
// 4 个 muduo IO 线程与 2 个 MQ 消费线程职责不同；启动时的 sleep 不检测依赖就绪。

#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#include"../include/ChatServer.h"

const std::string RABBITMQ_HOST = "localhost";
const std::string QUEUE_NAME = "sql_queue";
const int THREAD_NUM = 2;

// MQ 消费回调：执行队列消息中的 SQL；异常向 worker 传播，因此不会到达其 ACK 语句。
void executeMysql(const std::string sql) {
    http::MysqlUtil mysqlUtil_;
    mysqlUtil_.executeUpdate(sql);
}

// 临时创建 IPv4 socket 并试 bind；随后关闭，不预留端口，检查与正式绑定之间仍有竞争窗口。
static bool checkPortAvailable(int port) {

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "[ERROR] 无法创建测试 socket" << std::endl;
        return false;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    close(sock);
    return true;
}

// 启动顺序在同一主线程推进；参数 atoi 未校验格式/范围，外部依赖构造异常也未统一捕获。
int main(int argc, char* argv[]) {
    LOG_INFO << "pid = " << getpid();

    std::string serverName = "ChatServer";
    int port = 8080;

    int opt;
    const char* str = "p:";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
        case 'p': {
            port = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }

    muduo::Logger::setLogLevel(muduo::Logger::WARN);

    if (!checkPortAvailable(port)) {
        std::cerr << "[ERROR] 端口 " << port << " 已被占用，无法启动服务！" << std::endl;
        std::cerr << "[提示] 1. 使用 -p <端口号> 指定其他端口" << std::endl;
        std::cerr << "[提示] 2. 执行 lsof -i :" << port << " 查看端口占用进程" << std::endl;
        exit(1);
    }
    std::cout << "[INFO] 服务启动在端口: " << port << std::endl;

    // 组装对象与业务设施；构造返回不等于已进入监听事件循环。
    ChatServer server(port, serverName);

    // 配置 muduo IO 线程数；MQ 消费线程数由 THREAD_NUM 单独设置。
    server.setThreadNum(4);

    // 仅暂停主线程，不验证 MySQL/RabbitMQ 就绪，不能当同步屏障。
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 在对外处理请求前同步恢复历史，可能使启动时间随数据量增长。
    server.initChatMessage();

    RabbitMQThreadPool pool(RABBITMQ_HOST, QUEUE_NAME, THREAD_NUM, executeMysql);
    // 启动独立消费者线程，成功处理 SQL 后由 worker ACK。
    pool.start();

    // 启动 TCP 服务并在当前主线程进入 EventLoop；循环退出后才继续 main。
    server.start();

    return 0;
}
