// tests/manual/coordinator_with_real_nodes_test.cpp
// 
// 사전 요구사항:
//   Terminal 1: ./node --id node_1
//   Terminal 2: ./node --id node_2
//   Terminal 3: ./node --id node_3
//
// 실행:
//   ./coordinator_with_real_nodes_test

#include "coordinator/CoordinatorServer.hpp"
#include "protocols/coordinator_node/include/SigningProtocol.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

using namespace mpc_engine;
using namespace mpc_engine::coordinator;
using namespace mpc_engine::node;
using namespace mpc_engine::protocol::coordinator_node;

void TestBasicConnection() {
    std::cout << "\n=== Test 1: Connect to Real Node Servers ===" << std::endl;
    
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    
    assert(coordinator.Initialize());
    assert(coordinator.Start());
    
    // 실제 실행 중인 Node 서버들 등록
    std::cout << "Registering nodes..." << std::endl;
    assert(coordinator.RegisterNode("node_1", PlatformType::LOCAL, "127.0.0.1", 8081, 0));
    assert(coordinator.RegisterNode("node_2", PlatformType::LOCAL, "127.0.0.1", 8082, 1));
    assert(coordinator.RegisterNode("node_3", PlatformType::LOCAL, "127.0.0.1", 8083, 2));
    std::cout << "✓ Nodes registered" << std::endl;
    
    // 연결
    std::cout << "\nConnecting to nodes..." << std::endl;
    assert(coordinator.ConnectToNode("node_1"));
    std::cout << "✓ Connected to node_1" << std::endl;
    
    assert(coordinator.ConnectToNode("node_2"));
    std::cout << "✓ Connected to node_2" << std::endl;
    
    assert(coordinator.ConnectToNode("node_3"));
    std::cout << "✓ Connected to node_3" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 연결 확인
    size_t connected = coordinator.GetConnectedNodeCount();
    std::cout << "\nConnected nodes: " << connected << "/3" << std::endl;
    assert(connected == 3);
    
    std::cout << "✓ Test 1 PASSED: All nodes connected" << std::endl;
}

void TestSingleRequest() {
    std::cout << "\n=== Test 2: Single Signing Request ===" << std::endl;
    
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    
    SigningRequest request;
    request.keyId = "test_key_001";
    request.transactionData = "0x1234567890abcdef";
    request.threshold = 2;
    request.totalShards = 3;
    
    std::cout << "Sending signing request to node_1..." << std::endl;
    std::cout << "  Key ID: " << request.keyId << std::endl;
    std::cout << "  Transaction: " << request.transactionData << std::endl;
    
    auto response = coordinator.SendToNode("node_1", &request);
    
    assert(response != nullptr);
    assert(response->success);
    
    std::cout << "✓ Response received successfully" << std::endl;
    std::cout << "✓ Test 2 PASSED: Single request works" << std::endl;
}

void TestBroadcast() {
    std::cout << "\n=== Test 3: Broadcast to All Nodes ===" << std::endl;
    
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    
    SigningRequest request;
    request.keyId = "broadcast_key_001";
    request.transactionData = "0xbroadcast_test_data";
    request.threshold = 2;
    request.totalShards = 3;
    
    std::vector<std::string> node_ids = {"node_1", "node_2", "node_3"};
    
    std::cout << "Broadcasting to 3 nodes..." << std::endl;
    auto start = std::chrono::steady_clock::now();
    
    bool result = coordinator.BroadcastToNodes(node_ids, &request);
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    assert(result);
    std::cout << "✓ Broadcast completed in " << ms << "ms" << std::endl;
    std::cout << "✓ Test 3 PASSED: Broadcast works" << std::endl;
}

void TestConcurrentRequests() {
    std::cout << "\n=== Test 4: Concurrent Requests to Same Node ===" << std::endl;
    
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    
    const int NUM_REQUESTS = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    std::cout << "Sending " << NUM_REQUESTS << " concurrent requests to node_1..." << std::endl;
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        threads.emplace_back([&coordinator, i, &success_count]() {
            SigningRequest request;
            request.keyId = "concurrent_key_" + std::to_string(i);
            request.transactionData = "0x" + std::to_string(i);
            request.threshold = 2;
            request.totalShards = 3;
            
            auto response = coordinator.SendToNode("node_1", &request);
            if (response && response->success) {
                success_count++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    std::cout << "✓ " << success_count.load() << "/" << NUM_REQUESTS 
              << " requests succeeded in " << ms << "ms" << std::endl;
    assert(success_count.load() == NUM_REQUESTS);
    std::cout << "✓ Test 4 PASSED: Concurrent requests work" << std::endl;
}

void TestReconnection() {
    std::cout << "\n=== Test 5: Disconnect and Reconnect ===" << std::endl;
    
    // TODO: Reconnection 기능은 Phase 8 (TLS/mTLS 적용) 시 함께 개선 예정
    // 이유:
    //   - 현재: 평문 TCP 기반으로 Queue 재생성만 필요
    //   - TLS 적용 후: SSL 세션 관리, 인증서 재검증, Handshake 등 추가 로직 필요
    //   - 지금 구현해도 TLS 추가 시 전면 수정 불가피
    // 
    // 현재 문제:
    //   - Disconnect 후 send_queue가 shutdown 상태로 남음
    //   - Reconnect 시 새로운 queue 생성 필요
    //
    // 해결 방법 (Phase 8에서):
    //   - NodeTcpClient::Connect()에서 send_queue 재생성
    //   - SSL 컨텍스트와 함께 통합 설계
    
    std::cout << "✓ Test 5 SKIPPED: Will be implemented in Phase 8 (TLS)" << std::endl;
}

void TestNodeStats() {
    std::cout << "\n=== Test 6: Coordinator Statistics ===" << std::endl;
    
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    
    CoordinatorStats stats = coordinator.GetStats();
    
    std::cout << "Coordinator Stats:" << std::endl;
    std::cout << "  Total Nodes: " << stats.total_nodes << std::endl;
    std::cout << "  Connected Nodes: " << stats.connected_nodes << std::endl;
    std::cout << "  Ready Nodes: " << stats.ready_nodes << std::endl;
    std::cout << "  Uptime: " << stats.uptime_seconds << "s" << std::endl;
    
    assert(stats.total_nodes == 3);
    assert(stats.connected_nodes == 3);
    
    std::vector<std::string> connected_nodes = coordinator.GetConnectedNodeIds();
    std::cout << "\nConnected Nodes:" << std::endl;
    for (const auto& node_id : connected_nodes) {
        std::cout << "  - " << node_id << " (" << coordinator.GetNodeEndpoint(node_id) << ")" << std::endl;
    }
    
    std::cout << "✓ Test 6 PASSED: Statistics work" << std::endl;
}

void Cleanup() {
    std::cout << "\n=== Cleanup ===" << std::endl;
    
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    
    std::cout << "Disconnecting from all nodes..." << std::endl;
    coordinator.DisconnectAllNodes();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    coordinator.Stop();
    std::cout << "✓ Coordinator stopped" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Coordinator with Real Nodes Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nPre-requisites:" << std::endl;
    std::cout << "  Make sure 3 Node servers are running:" << std::endl;
    std::cout << "    Terminal 1: ./node --port 8081" << std::endl;
    std::cout << "    Terminal 2: ./node --port 8082" << std::endl;
    std::cout << "    Terminal 3: ./node --port 8083" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Waiting 3 seconds for you to start nodes..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    try {
        TestBasicConnection();
        TestSingleRequest();
        TestBroadcast();
        TestConcurrentRequests();
        TestReconnection();
        TestNodeStats();
        
        Cleanup();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  Core tests (1-4, 6) PASSED!" << std::endl;
        std::cout << "  Test 5 deferred to Phase 8 (TLS)" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        Cleanup();
        return 1;
    }
}