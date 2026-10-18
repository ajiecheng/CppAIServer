# CppAIService — C++ AI 应用服务平台

基于 muduo 的 C++ HTTP 框架构建的 AI 应用服务平台。

## 项目架构

```
┌─────────────────────────────────────────────────────┐
│                    客户端层                         │
│          Web 浏览器 / 命令行 / 第三方 SDK            │
├─────────────────────────────────────────────────────┤
│                  业务服务层（C++）                   │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐             │
│  │ 对话服务  │ │ 图像识别  │ │ 语音服务 │            │
│  │ ChatHandler│ │ImageRecog│ │ ASR/TTS│             │
│  └──────────┘ └──────────┘ └──────────┘             │
│  ┌──────────────────────────────────────┐           │
│  │                 muduo                │           │
│  │    Router → Middleware → Session     │           │
│  └──────────────────────────────────────┘           │
├─────────────────────────────────────────────────────┤
│                数据与消息层                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐          │
│  │  MySQL   │  │ RabbitMQ │  │  Session  │          │
│  └──────────┘  └──────────┘  └──────────┘          │
├─────────────────────────────────────────────────────┤
│              推理与第三方平台层                      │
│  阿里百炼 · 火山豆包 · 百度 TTS · ONNX Runtime       │
└─────────────────────────────────────────────────────┘
```

## 核心功能

| 模块 | 说明 |
|------|------|
| **多模型对话** | 策略模式 + 注册式工厂，支持阿里百炼（通义千问）、豆包等模型一键切换 |
| **轻量级 MCP** | 配置化工具注册（`AIToolRegistry`）+ Prompt 协议化，实现两段式推理 |
| **RAG 检索增强** | 基于阿里百炼知识库，支持知识库 ID 配置化接入 |
| **图像识别** | ONNX Runtime + OpenCV DNN，MobileNetV2 本地推理 |
| **语音合成（TTS）** | 集成百度 TTS（任务创建 → 轮询 → 回传 URL），支持参数化语速/音色 |
| **多会话管理** | 单用户多会话隔离，`unordered_map<userId, map<sessionId, AIHelper>>` |
| **异步消息入库** | RabbitMQ 承载持久化写库，同步写内存 + 异步入库，避免主线程阻塞 |
| **CORS 中间件** | 内置跨域支持，可配置 |

## 技术栈

- **语言标准**：C++17
- **构建系统**：CMake 3.10+ / Ninja
- **HTTP 框架**：[C++ HTTP Server](HttpServer/)（基于 muduo 网络库）
- **数据库**：MySQL（通过 mysqlcppconn）
- **消息队列**：RabbitMQ（SimpleAmqpClient）
- **AI 推理**：libcurl（API 调用） + ONNX Runtime（本地推理）
- **图像处理**：OpenCV 4.x
- **语音**：百度语音 API
- **加密**：OpenSSL

## 目录结构

```
CppAIService/
├── AIApps/ChatServer/          # AI 应用服务主程序
│   ├── include/
│   │   ├── ChatServer.h        # 服务入口
│   │   ├── handlers/           # 请求处理器
│   │   └── AIUtil/             # AI 核心模块
│   │       ├── AIHelper.h      # 对话管理
│   │       ├── AIStrategy.h    # 多模型策略
│   │       ├── AIFactory.h     # 策略工厂
│   │       ├── AIConfig.h      # MCP 配置
│   │       ├── AIToolRegistry.h # 工具注册表
│   │       ├── ImageRecognizer.h # 图像识别
│   │       ├── AISpeechProcessor.h # 语音合成
│   │       └── base64.h        # Base64 编解码
│   ├── src/                    # 源码实现
│   └── resource/               # 静态资源 & 配置文件
├── HttpServer/                 # 自研 HTTP 框架
│   ├── include/
│   │   ├── http/               # HTTP 核心（请求/响应/上下文）
│   │   ├── router/             # 路由系统
│   │   ├── session/            # 会话管理
│   │   ├── middleware/          # 中间件链
│   │   ├── ssl/                # SSL/TLS
│   │   └── utils/              # 工具（MySQL/文件/JSON）
│   └── src/
├── CMakeLists.txt              # 构建配置
└── docs/                       # 文档
```

## 安装与编译

### 环境要求

- Ubuntu 20.04 / 22.04（或其他 Linux 发行版）
- GCC 9+ 或 Clang 10+（支持 C++17）
- CMake 3.10+
- MySQL 8.0+
- RabbitMQ 3.x

### 安装依赖

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

### 安装 ONNX Runtime

从 [ONNX Runtime Releases](https://github.com/microsoft/onnxruntime/releases) 下载 Linux 预编译包，将 `.so` 文件放入 `/usr/local/lib` 并执行 `sudo ldconfig`。

### 编译项目

```bash
git clone https://github.com/ajiecheng/CppAIService.git
cd CppAIService

# CMake 配置（可选禁用某些模块）
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

| 模型 | 环境变量 | 说明 |
|------|----------|------|
| 阿里百炼 | `DASHSCOPE_API_KEY` | [获取 Key](https://bailian.console.aliyun.com/) |
| 火山豆包 | `DOUBAO_API_KEY` | [获取 Key](https://console.volcengine.com/) |

如果环境变量未设置，程序会自动读取 `~/.cppaiservice/apikey.conf` 文件（第一行为 Key）。

### 百度 TTS

```bash
export BAIDU_CLIENT_ID="your_client_id"
export BAIDU_CLIENT_SECRET="your_client_secret"
```

### ONNX 模型

确保 [mobilenetv2-7.onnx](AIApps/ChatServer/resource/mobilenetv2-7.onnx) 和 [imagenet_classes.txt](AIApps/ChatServer/resource/imagenet_classes.txt) 存在于 `resource/` 目录下。

## 运行

```bash
# 默认端口 80（需 root）
sudo ./build/http_server

# 指定端口
./build/http_server -p 8080
```

启动后访问 `http://localhost:8080` 进入"卡码AI助手"Web 界面。

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
| POST | `/chat/tts` | 语音合成 |
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


