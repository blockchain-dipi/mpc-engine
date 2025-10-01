// tests/integration/coordinator_node_basic_test.cpp
#include "coordinator/CoordinatorServer.hpp"
#include "node/NodeServer.hpp"
#include "node/network/include/NodeTcpServer.hpp"
#include "protocols/coordinator_node/include/SigningProtocol.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <vector>
#include <memory>

using namespace mpc_engine;
using namespace mpc_engine::coordinator;
using namespace mpc_engine::node;
using namespace mpc_engine::node::network;
using namespace mpc_engine::protocol::coordinator_node;

void TestBasicConnection() {
    std::cout << "\n=== Test 1: Basic Connection ===" << std::endl;
    std::cout << "Testing: Coordinator connects to Node" << std::endl;
    
    // 1. Node 서버 시작
    NodeServer node(NodePlatformType::LOCAL);
    NodeConfig node_config;
    node_config.node_id = "test_node_1";
    node_config.bind_address = "127.0.0.1";
    node_config.bind_port = 19001;
    node_config.platform_type = NodePlatformType::LOCAL;
    
    node.SetNodeConfig(node_config);
    
    assert(node.Initialize());
    NodeTcpServer* tcp_server = node.GetTcpServer();
    if (tcp_server) {
        tcp_server->SetTrustedCoordinator("127.0.0.1");  // ← 추가
    }
    assert(node.Start());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[Node] Started on 127.0.0.1:19001" << std::endl;
    
    // 2. Coordinator 초기화
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    assert(coordinator.Initialize());
    assert(coordinator.Start());
    std::cout << "[Coordinator] Started" << std::endl;
    
    // 3. Node 등록
    assert(coordinator.RegisterNode("test_node_1", NodePlatformType::LOCAL, "127.0.0.1", 19001, 0));
    std::cout << "[Coordinator] Node registered" << std::endl;
    
    // 4. Node 연결
    assert(coordinator.ConnectToNode("test_node_1"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[Coordinator] Connected to Node" << std::endl;
    
    // 5. 연결 확인
    assert(coordinator.IsNodeConnected("test_node_1"));
    std::cout << "[Coordinator] Connection verified: " << coordinator.GetNodeEndpoint("test_node_1") << std::endl;
    
    // 6. 정리
    coordinator.DisconnectFromNode("test_node_1");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    node.Stop();
    
    std::cout << "✓ Test 1 passed: Basic connection works" << std::endl;
}

void TestSingleRequest() {
    std::cout << "\n=== Test 2: Single Request ===" << std::endl;
    std::cout << "Testing: Send signing request and receive response" << std::endl;
    
    // 1. Node 서버 시작
    NodeServer node(NodePlatformType::LOCAL);
    NodeConfig node_config;
    node_config.node_id = "test_node_2";
    node_config.bind_address = "127.0.0.1";
    node_config.bind_port = 19002;
    node_config.platform_type = NodePlatformType::LOCAL;
    
    node.SetNodeConfig(node_config);
    
    assert(node.Initialize());
    NodeTcpServer* tcp_server = node.GetTcpServer();
    if (tcp_server) {
        tcp_server->SetTrustedCoordinator("127.0.0.1");  // ← 추가
    }
    assert(node.Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 2. Coordinator
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    assert(coordinator.RegisterNode("test_node_2", NodePlatformType::LOCAL, "127.0.0.1", 19002, 0));
    assert(coordinator.ConnectToNode("test_node_2"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3. 서명 요청
    SigningRequest request;
    request.keyId = "test_key_123";
    request.transactionData = "0x1234567890abcdef";
    request.threshold = 2;
    request.totalShards = 3;
    
    std::cout << "[Coordinator] Sending signing request..." << std::endl;
    std::cout << "  Key ID: " << request.keyId << std::endl;
    std::cout << "  Transaction: " << request.transactionData << std::endl;
    
    auto response = coordinator.SendToNode("test_node_2", &request);
    
    assert(response != nullptr);
    assert(response->success);
    std::cout << "[Coordinator] Response received successfully" << std::endl;
    
    // 4. 정리
    coordinator.DisconnectFromNode("test_node_2");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    node.Stop();
    
    std::cout << "✓ Test 2 passed: Single request works" << std::endl;
}

void TestMultipleNodes() {
    std::cout << "\n=== Test 3: Multiple Nodes Broadcast ===" << std::endl;
    std::cout << "Testing: Broadcast to 3 nodes in parallel" << std::endl;
    
    // ✅ 이전 노드 정리
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    for (const auto& node_id : coordinator.GetAllNodeIds()) {
        coordinator.UnregisterNode(node_id);
    }
    
    // 1. 3개 Node 서버 시작
    std::vector<std::unique_ptr<NodeServer>> nodes;
    
    for (int i = 0; i < 3; ++i) {
        auto node = std::make_unique<NodeServer>(NodePlatformType::LOCAL);
        NodeConfig config;
        config.node_id = "bcast_" + std::to_string(i + 1);  // ✅ 짧고 명확한 이름
        config.bind_address = "127.0.0.1";
        config.bind_port = 19010 + i;
        config.platform_type = NodePlatformType::LOCAL;
        
        node->SetNodeConfig(config);
        
        assert(node->Initialize());
        NodeTcpServer* tcp_server = node->GetTcpServer();
        if (tcp_server) {
            tcp_server->SetTrustedCoordinator("127.0.0.1");
        } 
        assert(node->Start());
        std::cout << "[Node " << (i+1) << "] Started on 127.0.0.1:" << (19010 + i) << std::endl;
        
        nodes.push_back(std::move(node));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 2. Coordinator에 등록 및 연결
    for (int i = 0; i < 3; ++i) {
        std::string node_id = "bcast_" + std::to_string(i + 1);
        assert(coordinator.RegisterNode(node_id, NodePlatformType::LOCAL, "127.0.0.1", 19010 + i, i));
        assert(coordinator.ConnectToNode(node_id));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3. 연결 확인
    size_t connected = coordinator.GetConnectedNodeCount();
    std::cout << "[Coordinator] Connected to " << connected << " nodes" << std::endl;
    assert(connected == 3);
    
    // 4. Broadcast 테스트
    SigningRequest request;
    request.keyId = "broadcast_key";
    request.transactionData = "0xbroadcasttest";
    request.threshold = 2;
    request.totalShards = 3;
    
    std::vector<std::string> node_ids = {"bcast_1", "bcast_2", "bcast_3"};
    
    std::cout << "[Coordinator] Broadcasting signing request to 3 nodes..." << std::endl;
    auto start = std::chrono::steady_clock::now();
    
    bool result = coordinator.BroadcastToNodes(node_ids, &request);
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    assert(result);
    std::cout << "[Coordinator] Broadcast completed in " << ms << "ms" << std::endl;
    std::cout << "  (Parallel processing: 3 nodes received simultaneously)" << std::endl;
    
    // 5. 통계 확인
    CoordinatorStats stats = coordinator.GetStats();
    std::cout << "[Coordinator] Stats:" << std::endl;
    std::cout << "  Total nodes: " << stats.total_nodes << std::endl;
    std::cout << "  Connected nodes: " << stats.connected_nodes << std::endl;
    
    // 6. 정리
    for (int i = 0; i < 3; ++i) {
        coordinator.DisconnectFromNode("bcast_" + std::to_string(i + 1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    for (auto& node : nodes) {
        node->Stop();
    }
    
    std::cout << "✓ Test 3 passed: Broadcast works" << std::endl;
}

void TestConcurrentRequests() {
    std::cout << "\n=== Test 4: Concurrent Requests ===" << std::endl;
    std::cout << "Testing: Multiple simultaneous requests to same node" << std::endl;
    
    // ✅ 이전 노드 정리
    CoordinatorServer& coordinator = CoordinatorServer::Instance();
    for (const auto& node_id : coordinator.GetAllNodeIds()) {
        coordinator.UnregisterNode(node_id);
    }
    
    // 1. Node 서버 시작
    NodeServer node(NodePlatformType::LOCAL);
    NodeConfig node_config;
    node_config.node_id = "concurrent_node";  // ✅ 다른 이름
    node_config.bind_address = "127.0.0.1";
    node_config.bind_port = 19020;
    node_config.platform_type = NodePlatformType::LOCAL;
    
    node.SetNodeConfig(node_config);
    
    assert(node.Initialize());
    NodeTcpServer* tcp_server = node.GetTcpServer();
    if (tcp_server) {
        tcp_server->SetTrustedCoordinator("127.0.0.1");  // ← 추가
    }
    assert(node.Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 2. Coordinator
    assert(coordinator.RegisterNode("concurrent_node", NodePlatformType::LOCAL, "127.0.0.1", 19020, 0));
    assert(coordinator.ConnectToNode("concurrent_node"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3. 동시 요청 (10개)
    const int NUM_REQUESTS = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    std::cout << "[Coordinator] Sending " << NUM_REQUESTS << " concurrent requests..." << std::endl;
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        threads.emplace_back([&coordinator, i, &success_count]() {
            SigningRequest request;
            request.keyId = "key_" + std::to_string(i);
            request.transactionData = "0x" + std::to_string(i);
            request.threshold = 2;
            request.totalShards = 3;
            
            auto response = coordinator.SendToNode("concurrent_node", &request);
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
    
    std::cout << "[Coordinator] " << success_count.load() << "/" << NUM_REQUESTS 
              << " requests succeeded in " << ms << "ms" << std::endl;
    assert(success_count.load() == NUM_REQUESTS);
    
    // 4. 정리
    coordinator.DisconnectFromNode("concurrent_node");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    node.Stop();
    
    std::cout << "✓ Test 4 passed: Concurrent requests work" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Coordinator-Node Basic Tests" << std::endl;
    std::cout << "  (Node servers run within test code)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        TestBasicConnection();
        TestSingleRequest();
        TestMultipleNodes();
        TestConcurrentRequests();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  All 4 tests passed!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed: " << e.what() << std::endl;
        return 1;
    }
}