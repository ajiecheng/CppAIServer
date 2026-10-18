// test_client.cc：独立 HTTPS 手工验证示例：连接 127.0.0.1:443，握手、发送 GET 并输出响应。
// 未加入主 CMake 服务目标，也不是自动测试套件；未配置证书信任/主机名校验，不代表安全验证通过。

#include <iostream>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class HttpsClient
{
public:
    HttpsClient()
    {

        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        ctx_ = SSL_CTX_new(TLS_client_method());
        if (!ctx_)
        {
            throw std::runtime_error("Failed to create SSL context");
        }
    }

    ~HttpsClient()
    {
        if (ssl_) SSL_free(ssl_);
        if (ctx_) SSL_CTX_free(ctx_);
        if (sock_ > 0) close(sock_);
    }

    // 阻塞创建 IPv4 TCP 连接，再 SSL_connect；host 按 IPv4 地址解析，不执行 DNS 或证书身份校验。
    void connect(const std::string& host, int port)
    {

        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(host.c_str());

        if (::connect(sock_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("Failed to connect");
        }

        ssl_ = SSL_new(ctx_);
        if (!ssl_) {
            throw std::runtime_error("Failed to create SSL");
        }

        SSL_set_fd(ssl_, sock_);
        if (SSL_connect(ssl_) <= 0) {
            throw std::runtime_error("Failed to establish SSL connection");
        }

        std::cout << "Connected using: " << SSL_get_cipher(ssl_) << "\n";
    }

    // 拼接 GET 报文，经 SSL_write 发送，再循环 SSL_read 输出；未完整处理部分写和 TLS 重试。
    void sendGetRequest(const std::string& path)
    {
        std::cout << "发送请求出去sendGetRequest" << std::endl;
        std::string request =
            "GET " + path + " HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n"
            "\r\n";

        if (SSL_write(ssl_, request.c_str(), request.length()) <= 0)
        {
            throw std::runtime_error("Failed to send request");
        }

        char buffer[4096];
        int bytes;
        while ((bytes = SSL_read(ssl_, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes] = 0;
            std::cout << buffer;
        }
    }

private:
    SSL_CTX* ctx_ = nullptr;
    SSL* ssl_ = nullptr;
    int sock_ = -1;
};

// 手动连接本机 443 并访问根路径；需要另有正确启用 TLS 的服务端。
int main() {
    try {
        HttpsClient client;
        client.connect("127.0.0.1", 443);
        std::cout << "连接成功connect" << std::endl;
        client.sendGetRequest("/");
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
