# CppAIService
CppAIserver项目是基于muduo网络库，Reactor模型构建的HTTTP框架服务

## 核心功能
实现了登录注册、AI 对话、简易图像识别

## 技术栈
C++17、muduo、OpenSSL、MySQL、STL/RAII、多线程、HTTP/HTTPS

## 环境要求
- Ubuntu 20.04 / 22.04（或其他 Linux 发行版）
- GCC 9+ 或 Clang 10+（支持 C++17）
- CMake 3.10+
- MySQL 8.0+
- RabbitMQ 3.x

安装依赖
```bash
# 基础工具链
sudo apt update && sudo apt install -y build-essential cmake git ninja-build \
    pkg-config curl wget

# 系统库
sudo apt install -y libssl-dev libcurl4-openssl-dev libopencv-dev \
    libmysqlclient-dev libprotobuf-dev protobuf-compiler libboost-all-dev

# MySQL Connector/C++
sudo apt install -y libmysqlcppconn-dev

# RabbitMQ
sudo apt install -y librabbitmq-dev
```

### 编译 muduo（从源码）
```bash
cd /tmp
git clone https://github.com/chenshuo/muduo.git
cd muduo && mkdir -p build && cd build
cmake -G Ninja ..
ninja && sudo ninja install
```

### 编译 SimpleAmqpClient（从源码）
```bash
cd /tmp
git clone https://github.com/alanxz/SimpleAmqpClient.git
cd SimpleAmqpClient && mkdir -p build && cd build
cmake -G Ninja ..
ninja && sudo ninja install
```

### 编译项目
```bash
git clone https://github.com/ajiecheng/CppAIService.git
cd CppAIService

# CMake 配置
cmake -S . -B build -G Ninja
# 自定义选项: -DBUILD_WITH_MUDUO=ON -DBUILD_WITH_ONNX=ON -DBUILD_WITH_AMQP=ON -DBUILD_WITH_MYSQL=ON

# 编译
cmake --build build -j$(nproc)
```

## 配置
### 数据库
在 [ChatServer.cpp](AIApps/ChatServer/src/ChatServer.cpp#L37) 中配置 MySQL 连接信息：
```cpp
MysqlUtil::init("tcp://127.0.0.1:3306", "your_user", "your_password", "ChatHttpServer", 5);
```

### AI 模型 API Key

API Key 通过环境变量或配置文件加载（优先环境变量）：
| 阿里百炼 | `DASHSCOPE_API_KEY` | [获取 Key](https://bailian.console.aliyun.com/) |
| 火山豆包 | `DOUBAO_API_KEY` | [获取 Key](https://console.volcengine.com/) |
如果环境变量未设置，程序会自动读取 `~/.cppaiservice/apikey.conf` 文件（第一行为 Key）。

## 运行

```bash
# 默认端口 8080（需 root）
sudo ./build/http_server

# 指定端口
./build/http_server -p 8080
```

启动后访问 `http://localhost:8080` 进入"AI助手"Web 界面。

### API 路由

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` `/entry` | 入口页面 |
| POST | `/login` | 用户登录 |
| POST | `/register` | 用户注册 |
| POST | `/user/logout` | 用户登出 |
| GET | `/chat` | 聊天页面 |
| POST | `/chat/send` | 发送对话消息 |
| POST | `/chat/send-new-session` | 新建会话并发送 |
| GET | `/chat/sessions` | 获取会话列表 |
| POST | `/chat/history` | 获取历史消息 |
| GET | `/menu` | AI 功能菜单 |
| GET | `/upload` | 图片上传页面 |
| POST | `/upload/send` | 图片识别 |

## CMake 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_WITH_MUDUO` | ON | 启用 muduo HTTP 服务 |
| `BUILD_WITH_MYSQL` | ON | 启用 MySQL 支持 |
| `BUILD_WITH_ONNX` | ON | 启用 ONNX Runtime 推理 |
| `BUILD_WITH_AMQP` | ON | 启用 RabbitMQ 消息队列 |

## License


