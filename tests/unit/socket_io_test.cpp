// tests/unit/socket_io_test.cpp
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>
#include <thread>
#include <cassert>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <vector>

using namespace mpc_engine::utils;

// 테스트 헬퍼
void PrintTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

// 시그널 핸들러
void signal_handler(int) {}

// Test 1: 정상적인 송수신
bool TestNormalSendReceive() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);
    
    bind(server_sock, (sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 1);
    
    // Server thread
    std::thread server_thread([server_sock]() {
        int client = accept(server_sock, nullptr, nullptr);
        
        // 1000바이트 송신
        char send_buffer[1000];
        memset(send_buffer, 'A', sizeof(send_buffer));
        
        size_t bytes_sent = 0;
        SocketIOResult result = SendExact(client, send_buffer, 1000, &bytes_sent);
        
        assert(result == SocketIOResult::SUCCESS);
        assert(bytes_sent == 1000);
        
        close(client);
    });
    
    // Client
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    connect(client_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    
    // 1000바이트 수신
    char recv_buffer[1000];
    size_t bytes_received = 0;
    SocketIOResult result = ReceiveExact(client_sock, recv_buffer, 1000, &bytes_received);
    
    assert(result == SocketIOResult::SUCCESS);
    assert(bytes_received == 1000);
    assert(recv_buffer[0] == 'A' && recv_buffer[999] == 'A');
    
    close(client_sock);
    server_thread.join();
    close(server_sock);
    
    return true;
}

// Test 2: 시그널 인터럽트 처리
bool TestSignalInterrupt() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9001);
    
    bind(server_sock, (sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 1);
    
    // Server thread (천천히 송신)
    std::thread server_thread([server_sock]() {
        int client = accept(server_sock, nullptr, nullptr);
        
        char send_buffer[1000];
        memset(send_buffer, 'B', sizeof(send_buffer));
        
        // 100바이트씩 나눠서 송신
        for (int i = 0; i < 10; ++i) {
            send(client, send_buffer + (i * 100), 100, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        close(client);
    });
    
    // Client
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9001);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    connect(client_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    
    // 시그널 핸들러 설정
    signal(SIGALRM, signal_handler);
    
    // 시그널 발생 스레드
    std::thread signal_thread([client_sock]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pthread_kill(pthread_self(), SIGALRM);
    });
    
    // 1000바이트 수신 (시그널 발생해도 완료되어야 함)
    char recv_buffer[1000];
    size_t bytes_received = 0;
    SocketIOResult result = ReceiveExact(client_sock, recv_buffer, 1000, &bytes_received);
    
    // EINTR이 발생해도 ReceiveExact가 재시도하여 완료
    assert(result == SocketIOResult::SUCCESS);
    assert(bytes_received == 1000);
    
    signal_thread.join();
    close(client_sock);
    server_thread.join();
    close(server_sock);
    
    return true;
}

// Test 3: 연결 종료 감지
bool TestConnectionClosed() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9002);
    
    bind(server_sock, (sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 1);
    
    // Server thread (500바이트만 보내고 종료)
    std::thread server_thread([server_sock]() {
        int client = accept(server_sock, nullptr, nullptr);
        
        char send_buffer[500];
        memset(send_buffer, 'C', sizeof(send_buffer));
        
        send(client, send_buffer, 500, 0);
        
        // 즉시 연결 종료 (FIN 전송)
        shutdown(client, SHUT_WR);
        close(client);
    });
    
    // Client
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9002);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    connect(client_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    
    // 1000바이트 요청하지만 500바이트만 받고 연결 종료
    char recv_buffer[1000];
    size_t bytes_received = 0;
    SocketIOResult result = ReceiveExact(client_sock, recv_buffer, 1000, &bytes_received);
    
    assert(result == SocketIOResult::CONNECTION_CLOSED);
    assert(bytes_received == 500);  // 부분 수신
    
    close(client_sock);
    server_thread.join();
    close(server_sock);
    
    return true;
}

// Test 4: 타임아웃 처리
bool TestTimeout() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9003);
    
    bind(server_sock, (sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 1);
    
    // Server thread (500바이트만 보내고 대기)
    std::thread server_thread([server_sock]() {
        int client = accept(server_sock, nullptr, nullptr);
        
        char send_buffer[500];
        memset(send_buffer, 'D', sizeof(send_buffer));
        
        send(client, send_buffer, 500, 0);
        
        // 나머지는 안 보내고 대기
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        close(client);
    });
    
    // Client
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // 2초 타임아웃 설정
    SetSocketRecvTimeout(client_sock, 2000);
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9003);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    connect(client_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    
    // 1000바이트 요청하지만 타임아웃
    char recv_buffer[1000];
    size_t bytes_received = 0;
    auto start = std::chrono::steady_clock::now();
    SocketIOResult result = ReceiveExact(client_sock, recv_buffer, 1000, &bytes_received);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    assert(result == SocketIOResult::TIMEOUT);
    assert(bytes_received == 500);  // 부분 수신
    
    // 타임아웃이 작동했는지 확인 (약 2초)
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    assert(elapsed_ms >= 2000 && elapsed_ms < 3000);
    
    close(client_sock);
    server_thread.join();
    close(server_sock);
    
    return true;
}

// Test 5: 대용량 데이터 송수신
bool TestLargeData() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9004);
    
    bind(server_sock, (sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 1);
    
    const size_t DATA_SIZE = 10 * 1024 * 1024;  // 10MB
    
    // Server thread
    std::thread server_thread([server_sock, DATA_SIZE]() {
        int client = accept(server_sock, nullptr, nullptr);
        
        std::vector<char> send_buffer(DATA_SIZE);
        for (size_t i = 0; i < DATA_SIZE; ++i) {
            send_buffer[i] = static_cast<char>(i % 256);
        }
        
        size_t bytes_sent = 0;
        SocketIOResult result = SendExact(client, send_buffer.data(), DATA_SIZE, &bytes_sent);
        
        assert(result == SocketIOResult::SUCCESS);
        assert(bytes_sent == DATA_SIZE);
        
        close(client);
    });
    
    // Client
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9004);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    connect(client_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    
    std::vector<char> recv_buffer(DATA_SIZE);
    size_t bytes_received = 0;
    auto start = std::chrono::steady_clock::now();
    SocketIOResult result = ReceiveExact(client_sock, recv_buffer.data(), DATA_SIZE, &bytes_received);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    assert(result == SocketIOResult::SUCCESS);
    assert(bytes_received == DATA_SIZE);
    
    // 데이터 검증
    for (size_t i = 0; i < DATA_SIZE; ++i) {
        assert(recv_buffer[i] == static_cast<char>(i % 256));
    }
    
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "  10MB transferred in " << elapsed_ms << "ms" << std::endl;
    
    close(client_sock);
    server_thread.join();
    close(server_sock);
    
    return true;
}

// Test 6: 헬퍼 함수 테스트
bool TestHelperFunctions() {
    // ToString 테스트
    assert(std::string(SocketIOResultToString(SocketIOResult::SUCCESS)) == "SUCCESS");
    assert(std::string(SocketIOResultToString(SocketIOResult::CONNECTION_CLOSED)) == "CONNECTION_CLOSED");
    assert(std::string(SocketIOResultToString(SocketIOResult::TIMEOUT)) == "TIMEOUT");
    
    // IsFatalError 테스트
    assert(!IsFatalError(SocketIOResult::SUCCESS));
    assert(!IsFatalError(SocketIOResult::INTERRUPTED));
    assert(IsFatalError(SocketIOResult::CONNECTION_CLOSED));
    assert(IsFatalError(SocketIOResult::CONNECTION_ERROR));
    
    // IsRetryable 테스트
    assert(!IsRetryable(SocketIOResult::SUCCESS));
    assert(IsRetryable(SocketIOResult::INTERRUPTED));
    assert(IsRetryable(SocketIOResult::TIMEOUT));
    assert(!IsRetryable(SocketIOResult::CONNECTION_ERROR));
    
    return true;
}

int main() {
    std::cout << "=== SocketIO (SendExact/ReceiveExact) Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        PrintTestResult("Normal Send/Receive", TestNormalSendReceive());
        PrintTestResult("Signal Interrupt Handling", TestSignalInterrupt());
        PrintTestResult("Connection Closed Detection", TestConnectionClosed());
        PrintTestResult("Timeout Handling", TestTimeout());
        PrintTestResult("Large Data Transfer", TestLargeData());
        PrintTestResult("Helper Functions", TestHelperFunctions());
        
        std::cout << std::endl;
        std::cout << "=== All SocketIO Tests Passed ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}