// tests/integration/node_tcp_server_test.cpp
#include "node/network/include/NodeTcpServer.hpp"
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace mpc_engine::node::network;
using namespace mpc_engine::protocol::coordinator_node;

// CoordinatorSimulator: Coordinator의 동작을 시뮬레이션하여 Node 서버를 테스트
// - Node 서버는 순수 서버로 요청을 받고 응답만 보냄
// - Coordinator는 클라이언트로 Node에 요청을 보내고 응답을 받음
// - 이 클래스는 Coordinator처럼 동작하여 Node 서버를 테스트함
class CoordinatorSimulator {
private:
    int sock = -1;
    std::string node_ip;
    uint16_t node_port;

public:
    CoordinatorSimulator(const std::string& ip, uint16_t port) 
        : node_ip(ip), node_port(port) {}
    
    ~CoordinatorSimulator() {
        Disconnect();
    }

    // Coordinator → Node: 연결
    bool Connect() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return false;

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(node_port);
        inet_pton(AF_INET, node_ip.c_str(), &server_addr.sin_addr);

        if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sock);
            sock = -1;
            return false;
        }

        return true;
    }

    void Disconnect() {
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
    }

    // Coordinator → Node: 서명 요청 전송 (예: SigningRequest)
    bool SendRequestToNode(const NetworkMessage& msg) {
        if (send(sock, &msg.header, sizeof(MessageHeader), 0) <= 0) {
            return false;
        }
        if (msg.header.body_length > 0) {
            if (send(sock, msg.body.data(), msg.header.body_length, 0) <= 0) {
                return false;
            }
        }
        return true;
    }

    // Coordinator ← Node: 서명 응답 수신 (예: SigningResponse)
    bool ReceiveResponseFromNode(NetworkMessage& msg) {
        ssize_t received = recv(sock, &msg.header, sizeof(MessageHeader), MSG_WAITALL);
        if (received != sizeof(MessageHeader)) {
            return false;
        }

        if (msg.header.body_length > 0) {
            msg.body.resize(msg.header.body_length);
            received = recv(sock, msg.body.data(), msg.header.body_length, MSG_WAITALL);
            if (received != static_cast<ssize_t>(msg.header.body_length)) {
                return false;
            }
        }

        return true;
    }
};

void TestBasicConnection() {
    std::cout << "\n=== Test 1: Basic Connection ===" << std::endl;
    std::cout << "목적: Coordinator가 Node에 연결하고 서명 요청/응답 테스트" << std::endl;

    NodeTcpServer server("127.0.0.1", 8081, 4);
    
    // Node 서버의 메시지 핸들러: 요청 받아서 응답 생성
    server.SetMessageHandler([](const NetworkMessage& request) -> NetworkMessage {
        std::cout << "[Node] Received signing request, type: " << request.header.message_type << std::endl;
        return NetworkMessage(request.header.message_type, "Signature_OK");
    });

    server.SetConnectedHandler([](const NodeConnectionInfo& info) {
        std::cout << "[Node] Coordinator connected: " << info.ToString() << std::endl;
    });

    server.SetDisconnectedHandler([](const NodeConnectionInfo& info) {
        std::cout << "[Node] Coordinator disconnected: " << info.ToString() << std::endl;
    });

    server.SetTrustedCoordinator("127.0.0.1");

    assert(server.Initialize());
    assert(server.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Coordinator 시뮬레이터: Node에 연결
    CoordinatorSimulator coordinator("127.0.0.1", 8081);
    assert(coordinator.Connect());
    std::cout << "[Coordinator] Connected to Node server" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Coordinator → Node: 서명 요청
    NetworkMessage request(0, "SigningRequest");
    std::cout << "[Coordinator] Sending signing request to Node..." << std::endl;
    assert(coordinator.SendRequestToNode(request));

    // Coordinator ← Node: 서명 응답
    NetworkMessage response;
    assert(coordinator.ReceiveResponseFromNode(response));
    std::cout << "[Coordinator] Received response from Node: " << response.GetBodyAsString() << std::endl;
    assert(response.GetBodyAsString() == "Signature_OK");

    coordinator.Disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    server.Stop();

    std::cout << "✓ Basic connection test passed" << std::endl;
}

void TestMultipleMessages() {
    std::cout << "\n=== Test 2: Multiple Messages ===" << std::endl;
    std::cout << "목적: Coordinator가 여러 서명 요청을 순차적으로 보내는 테스트" << std::endl;

    NodeTcpServer server("127.0.0.1", 8082, 4);
    
    std::atomic<int> message_count{0};
    
    // Node: 서명 요청 처리 시뮬레이션
    server.SetMessageHandler([&message_count](const NetworkMessage& request) -> NetworkMessage {
        int count = ++message_count;
        std::cout << "[Node] Processing signing request #" << count << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 서명 처리 시뮬레이션
        return NetworkMessage(request.header.message_type, "Signature_" + std::to_string(count));
    });

    server.SetTrustedCoordinator("127.0.0.1");

    assert(server.Initialize());
    assert(server.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    CoordinatorSimulator coordinator("127.0.0.1", 8082);
    assert(coordinator.Connect());
    std::cout << "[Coordinator] Connected to Node" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const int num_messages = 10;
    std::cout << "[Coordinator] Sending " << num_messages << " signing requests..." << std::endl;
    
    for (int i = 0; i < num_messages; i++) {
        NetworkMessage request(0, "SigningRequest_" + std::to_string(i));
        assert(coordinator.SendRequestToNode(request));
    }

    std::cout << "[Coordinator] Receiving " << num_messages << " responses..." << std::endl;
    for (int i = 0; i < num_messages; i++) {
        NetworkMessage response;
        assert(coordinator.ReceiveResponseFromNode(response));
        std::cout << "[Coordinator] Received: " << response.GetBodyAsString() << std::endl;
    }

    assert(message_count.load() == num_messages);

    auto stats = server.GetStats();
    std::cout << "\n[Node Stats]" << std::endl;
    std::cout << "  Requests received: " << stats.messages_received << std::endl;
    std::cout << "  Responses sent: " << stats.messages_sent << std::endl;
    std::cout << "  Pending in send queue: " << stats.pending_send_queue << std::endl;

    coordinator.Disconnect();
    server.Stop();

    std::cout << "✓ Multiple messages test passed" << std::endl;
}

void TestConcurrentProcessing() {
    std::cout << "\n=== Test 3: Concurrent Processing ===" << std::endl;
    std::cout << "목적: Node의 ThreadPool이 여러 서명 요청을 병렬로 처리하는지 테스트" << std::endl;

    NodeTcpServer server("127.0.0.1", 8083, 8); // 8 handler threads
    
    std::atomic<int> concurrent_count{0};
    std::atomic<int> max_concurrent{0};
    
    // Node: 서명 처리 중 동시 실행 개수 측정
    server.SetMessageHandler([&](const NetworkMessage& request) -> NetworkMessage {
        int current = ++concurrent_count;
        int max_val = max_concurrent.load();
        while (current > max_val && !max_concurrent.compare_exchange_weak(max_val, current)) {
            max_val = max_concurrent.load();
        }
        
        std::cout << "[Node] Handler processing (concurrent: " << current << ")" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 무거운 서명 작업 시뮬레이션
        
        --concurrent_count;
        return NetworkMessage(request.header.message_type, "Signature_OK");
    });

    server.SetTrustedCoordinator("127.0.0.1");

    assert(server.Initialize());
    assert(server.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    CoordinatorSimulator coordinator("127.0.0.1", 8083);
    assert(coordinator.Connect());
    std::cout << "[Coordinator] Connected to Node" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Coordinator: 많은 요청을 빠르게 전송
    const int num_messages = 20;
    std::cout << "[Coordinator] Sending " << num_messages << " requests quickly..." << std::endl;
    for (int i = 0; i < num_messages; i++) {
        NetworkMessage request(0, "SigningRequest");
        assert(coordinator.SendRequestToNode(request));
    }

    // Coordinator: 모든 응답 수신
    std::cout << "[Coordinator] Receiving all responses..." << std::endl;
    for (int i = 0; i < num_messages; i++) {
        NetworkMessage response;
        assert(coordinator.ReceiveResponseFromNode(response));
    }

    std::cout << "\n[Node] Max concurrent handlers: " << max_concurrent.load() << std::endl;
    assert(max_concurrent.load() > 1); // 병렬 처리 확인

    coordinator.Disconnect();
    server.Stop();

    std::cout << "✓ Concurrent processing test passed" << std::endl;
}

void TestSecurityRejection() {
    std::cout << "\n=== Test 4: Security Rejection ===" << std::endl;
    std::cout << "목적: Node가 신뢰하지 않는 IP로부터의 연결을 거부하는지 테스트" << std::endl;

    NodeTcpServer server("127.0.0.1", 8084, 4);
    
    server.SetMessageHandler([](const NetworkMessage& request) -> NetworkMessage {
        return NetworkMessage(request.header.message_type, "Signature_OK");
    });

    // Node는 192.168.1.1만 신뢰 (localhost 아님)
    server.SetTrustedCoordinator("192.168.1.1");
    std::cout << "[Node] Only trusting Coordinator IP: 192.168.1.1" << std::endl;

    assert(server.Initialize());
    assert(server.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 신뢰하지 않는 IP(127.0.0.1)에서 연결 시도
    CoordinatorSimulator untrusted_coordinator("127.0.0.1", 8084);
    std::cout << "[Untrusted Coordinator] Attempting to connect from 127.0.0.1..." << std::endl;
    bool connected = untrusted_coordinator.Connect();
    
    // 연결은 되지만 Node가 즉시 끊음
    if (connected) {
        std::cout << "[Untrusted Coordinator] TCP connected, but waiting for rejection..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        NetworkMessage request(0, "SigningRequest");
        bool sent = untrusted_coordinator.SendRequestToNode(request);
        
        std::cout << "[Node] Connection rejected by security policy" << std::endl;
        // Node가 연결을 끊었으므로 전송 실패해야 함
        assert(!sent);
    }

    untrusted_coordinator.Disconnect();
    server.Stop();

    std::cout << "✓ Security rejection test passed" << std::endl;
}

void TestGracefulShutdown() {
    std::cout << "\n=== Test 5: Graceful Shutdown ===" << std::endl;
    std::cout << "목적: Node가 처리 중인 서명 작업을 완료하고 안전하게 종료되는지 테스트" << std::endl;

    NodeTcpServer server("127.0.0.1", 8085, 4);
    
    std::atomic<int> completed_messages{0};
    
    // Node: 서명 처리에 시간이 걸림
    server.SetMessageHandler([&](const NetworkMessage& request) -> NetworkMessage {
        std::cout << "[Node] Processing signing request..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        completed_messages++;
        std::cout << "[Node] Signing completed (total: " << completed_messages.load() << ")" << std::endl;
        return NetworkMessage(request.header.message_type, "Signature_OK");
    });

    server.SetTrustedCoordinator("127.0.0.1");

    assert(server.Initialize());
    assert(server.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    CoordinatorSimulator coordinator("127.0.0.1", 8085);
    assert(coordinator.Connect());
    std::cout << "[Coordinator] Connected to Node" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Coordinator: 5개 요청 전송
    std::cout << "[Coordinator] Sending 5 signing requests..." << std::endl;
    for (int i = 0; i < 5; i++) {
        NetworkMessage request(0, "SigningRequest");
        assert(coordinator.SendRequestToNode(request));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Node: 처리 중에 종료
    std::cout << "\n[Node] Stopping server while processing..." << std::endl;
    server.Stop();

    std::cout << "[Node] Completed messages before shutdown: " << completed_messages.load() << std::endl;
    // 일부 메시지는 처리되어야 함
    assert(completed_messages.load() > 0);

    coordinator.Disconnect();

    std::cout << "✓ Graceful shutdown test passed" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  NodeTcpServer Integration Tests" << std::endl;
    std::cout << "  테스트 목적: Node 서버가 Coordinator의" << std::endl;
    std::cout << "  요청을 올바르게 처리하는지 검증" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n역할:" << std::endl;
    std::cout << "  • Node 서버 = 순수 서버 (요청 받고 응답만 보냄)" << std::endl;
    std::cout << "  • Coordinator = 클라이언트 (요청 보내고 응답 받음)" << std::endl;
    std::cout << "  • CoordinatorSimulator = 테스트용 Coordinator" << std::endl;
    std::cout << "========================================\n" << std::endl;

    try {
        TestBasicConnection();
        TestMultipleMessages();
        TestConcurrentProcessing();
        TestSecurityRejection();
        TestGracefulShutdown();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All tests passed! ✓" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}