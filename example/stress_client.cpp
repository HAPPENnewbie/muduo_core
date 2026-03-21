// 用于对服务器进行压力测试的 C++ 客户端
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>

class StressClient {
private:
    std::atomic<long> total_requests{0};  // 总成功请求数（原子变量，多线程安全）
    std::atomic<long> failed_requests{0}; // 失败请求数（连接/收发失败）
    std::atomic<long> total_latency_us{0};   // 总延迟（微秒）
    
public:
    void client_task(int client_id, int requests_per_client) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);  // 1. 创建 TCP socket
        // 2. 配置服务器地址
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;  
        server_addr.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
        // 3. 连接服务器
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            failed_requests++;
            return;
        }
        // 4. 循环发送指定数量的请求
        const char* msg = "hello muduo";
        for (int i = 0; i < requests_per_client; i++) {
            auto start = std::chrono::high_resolution_clock::now();
            
            send(sock, msg, strlen(msg), 0);
            char buf[1024] = {0};
            recv(sock, buf, sizeof(buf), 0);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            
            total_requests++;
            total_latency_us += latency;
        }
        
        close(sock);
    }
    
    void run(int num_clients, int requests_per_client) {
        std::vector<std::thread> threads;
        auto start_time = std::chrono::steady_clock::now();
        // 1. 创建指定数量的客户端线程
        for (int i = 0; i < num_clients; i++) {
            threads.emplace_back(&StressClient::client_task, this, i, requests_per_client);
        }
        // 2. 等待所有线程执行完毕（阻塞主线程）
        for (auto& t : threads) {
            t.join();
        }
        // 3. 计算测试总耗时（秒）
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
        
        // 输出统计报告（英文，避免编码问题）
        std::cout << "========== Stress Test Report ==========" << std::endl;
        std::cout << "Concurrent Clients: " << num_clients << std::endl;
        std::cout << "Total Requests: " << total_requests.load() << std::endl; // 总请求数
        std::cout << "Failed Requests: " << failed_requests.load() << std::endl;
        std::cout << "Test Duration: " << duration << " seconds" << std::endl;
        std::cout << "QPS: " << (duration > 0 ? total_requests.load() / duration : 0) << std::endl;  // QPS（每秒请求数）
        std::cout << "Average Latency: " << (total_requests.load() > 0 ?  total_latency_us.load() / total_requests.load() : 0) << " us" << std::endl; // 平均延迟
        std::cout << "=========================================" << std::endl;
    }
};

int main() {
    StressClient client;
    // 50 并发客户端，每个发送 1000 个请求
    client.run(10000, 1);
    return 0;
}