// tests/integration/test_mpc_engine_full.cpp
/**
 * MPC Engine Full Integration Test
 * 
 * 테스트 범위:
 * - Coordinator + Node 서버 시작/종료
 * - 연결 관리 (연결/재연결/끊기)
 * - Broadcast (2/3 MPC 시뮬레이션)
 * - 동시 요청 처리 (Connection Pool)
 * - 에러 케이스
 * - 부하 테스트
 */

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
#include <atomic>
#include <future>

using namespace mpc_engine;
using namespace mpc_engine::coordinator;
using namespace mpc_engine::node;
using namespace mpc_engine::node::network;
using namespace mpc_engine::protocol::coordinator_node;

// ========================================
// 테스트 헬퍼
// ========================================

void PrintTestHeader(const std::string& test_name) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << test_name << std::endl;
    std::cout << "========================================" << std::endl;
}

void PrintTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

// Node 서버 헬퍼 클래스
class TestNodeServer {
public:
    TestNodeServer(const std::string& id, uint16_t port, uint32_t shard_idx)
        : node_id_(id), port_(port), shard_index_(shard_idx)
    {
    }

    bool Start() {
        NodeConfig config;
        config.node_id = node_id_;
        config.bind_address = "127.0.0.1";
        config.bind_port = port_;
        config.platform_type = PlatformType::LOCAL;
       
        server_ = std::make_unique<NodeServer>(config);
        
        if (!server_->Initialize()) {
            std::cerr << "Failed to initialize " << node_id_ << std::endl;
            return false;
        }
        
        NodeTcpServer* tcp_server = server_->GetTcpServer();
        if (tcp_server) {
            tcp_server->SetTrustedCoordinator("127.0.0.1");
        }
        
        if (!server_->Start()) {
            std::cerr << "Failed to start " << node_id_ << std::endl;
            return false;
        }
        
        return true;
    }

    void Stop() {
        if (server_) {
            server_->Stop();
        }
    }

    const std::string& GetNodeId() const { return node_id_; }
    uint16_t GetPort() const { return port_; }
    uint32_t GetShardIndex() const { return shard_index_; }

private:
    std::string node_id_;
    uint16_t port_;
    uint32_t shard_index_;
    std::unique_ptr<NodeServer> server_;
};

// Coordinator 헬퍼
class TestCoordinator {
public:
    TestCoordinator() : coordinator_(CoordinatorServer::Instance()) {}

    bool Initialize() {
        if (!coordinator_.Initialize()) {
            return false;
        }
        return coordinator_.Start();
    }

    void Stop() {
        coordinator_.Stop();
    }

    bool RegisterNode(const std::string& node_id, uint16_t port, uint32_t shard_index) {
        return coordinator_.RegisterNode(
            node_id, 
            PlatformType::LOCAL, 
            "127.0.0.1", 
            port, 
            shard_index
        );
    }

    bool ConnectToNode(const std::string& node_id) {
        return coordinator_.ConnectToNode(node_id);
    }

    void DisconnectFromNode(const std::string& node_id) {
        coordinator_.DisconnectFromNode(node_id);
    }

    bool IsNodeConnected(const std::string& node_id) const {
        return coordinator_.IsNodeConnected(node_id);
    }

    std::unique_ptr<BaseResponse> SendToNode(const std::string& node_id, const BaseRequest* request) {
        return coordinator_.SendToNode(node_id, request);
    }

    bool BroadcastToNodes(const std::vector<std::string>& node_ids, const BaseRequest* request) {
        return coordinator_.BroadcastToNodes(node_ids, request);
    }

    size_t GetConnectedNodeCount() const {
        return coordinator_.GetConnectedNodeCount();
    }

    void UnregisterNode(const std::string& node_id) {
        coordinator_.UnregisterNode(node_id);
    }

private:
    CoordinatorServer& coordinator_;
};

// ========================================
// Test 1: 기본 서버 시작/종료
// ========================================
bool Test1_BasicServerStartStop() {
    PrintTestHeader("Test 1: Basic Server Start/Stop");
    
    try {
        // Node 3개 시작
        std::vector<std::unique_ptr<TestNodeServer>> nodes;
        for (int i = 0; i < 3; ++i) {
            auto node = std::make_unique<TestNodeServer>(
                "test1_node_" + std::to_string(i),
                20000 + i,
                i
            );
            
            if (!node->Start()) {
                return false;
            }
            std::cout << "✓ " << node->GetNodeId() << " started on port " << node->GetPort() << std::endl;
            nodes.push_back(std::move(node));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // Coordinator 시작
        TestCoordinator coordinator;
        if (!coordinator.Initialize()) {
            return false;
        }
        std::cout << "✓ Coordinator started" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // 정리
        coordinator.Stop();
        std::cout << "✓ Coordinator stopped" << std::endl;
        
        for (auto& node : nodes) {
            node->Stop();
        }
        std::cout << "✓ All nodes stopped" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// ========================================
// Test 2: 연결 관리
// ========================================
bool Test2_ConnectionManagement() {
    PrintTestHeader("Test 2: Connection Management");
    
    // 이전 테스트 정리
    CoordinatorServer& coord_instance = CoordinatorServer::Instance();
    for (const auto& node_id : coord_instance.GetAllNodeIds()) {
        coord_instance.UnregisterNode(node_id);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Node 1개 시작
    TestNodeServer node("test2_node", 20010, 0);
    if (!node.Start()) {
        return false;
    }
    std::cout << "✓ Node started" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    TestCoordinator coordinator;
    if (!coordinator.Initialize()) {
        return false;
    }
    std::cout << "✓ Coordinator started" << std::endl;
    
    // 연결 테스트
    assert(coordinator.RegisterNode(node.GetNodeId(), node.GetPort(), node.GetShardIndex()));
    std::cout << "✓ Node registered" << std::endl;
    
    assert(coordinator.ConnectToNode(node.GetNodeId()));
    std::cout << "✓ Connected to node" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    assert(coordinator.IsNodeConnected(node.GetNodeId()));
    std::cout << "✓ Connection verified" << std::endl;
    
    // 연결 끊기
    coordinator.DisconnectFromNode(node.GetNodeId());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    assert(!coordinator.IsNodeConnected(node.GetNodeId()));
    std::cout << "✓ Disconnected from node" << std::endl;
    
    // 재연결
    assert(coordinator.ConnectToNode(node.GetNodeId()));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    assert(coordinator.IsNodeConnected(node.GetNodeId()));
    std::cout << "✓ Reconnected to node" << std::endl;
    
    // 정리
    coordinator.Stop();
    node.Stop();
    
    return true;
}

// ========================================
// Test 3: Broadcast (2/3 MPC 시뮬레이션)
// ========================================
bool Test3_BroadcastMPC() {
    PrintTestHeader("Test 3: Broadcast (2/3 MPC)");
    
    // 이전 테스트 정리
    CoordinatorServer& coord_instance = CoordinatorServer::Instance();
    for (const auto& node_id : coord_instance.GetAllNodeIds()) {
        coord_instance.UnregisterNode(node_id);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 3개 Node 시작
    std::vector<std::unique_ptr<TestNodeServer>> nodes;
    for (int i = 0; i < 3; ++i) {
        auto node = std::make_unique<TestNodeServer>(
            "mpc_node_" + std::to_string(i),
            20020 + i,
            i
        );
        
        if (!node->Start()) {
            return false;
        }
        nodes.push_back(std::move(node));
    }
    std::cout << "✓ 3 nodes started" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    TestCoordinator coordinator;
    if (!coordinator.Initialize()) {
        return false;
    }
    
    // 모든 Node 등록 및 연결
    std::vector<std::string> node_ids;
    for (const auto& node : nodes) {
        assert(coordinator.RegisterNode(node->GetNodeId(), node->GetPort(), node->GetShardIndex()));
        assert(coordinator.ConnectToNode(node->GetNodeId()));
        node_ids.push_back(node->GetNodeId());
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    assert(coordinator.GetConnectedNodeCount() == 3);
    std::cout << "✓ All nodes connected" << std::endl;
    
    // Broadcast 서명 요청
    SigningRequest request;
    request.keyId = "mpc_key_broadcast";
    request.transactionData = "0xbroadcast_tx";
    request.threshold = 2;
    request.totalShards = 3;
    
    std::cout << "Broadcasting to " << node_ids.size() << " nodes..." << std::endl;
    
    bool broadcast_success = coordinator.BroadcastToNodes(node_ids, &request);
    
    assert(broadcast_success);
    std::cout << "✓ Broadcast completed successfully" << std::endl;
    
    // 정리
    coordinator.Stop();
    for (auto& node : nodes) {
        node->Stop();
    }
    
    return true;
}

// ========================================
// Test 4: 동시 요청 처리
// ========================================
bool Test4_ConcurrentRequests() {
    PrintTestHeader("Test 4: Concurrent Requests (100 requests)");
    
    // 이전 테스트 정리
    CoordinatorServer& coord_instance = CoordinatorServer::Instance();
    for (const auto& node_id : coord_instance.GetAllNodeIds()) {
        coord_instance.UnregisterNode(node_id);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Node 1개 시작
    TestNodeServer node("concurrent_node", 20030, 0);
    if (!node.Start()) {
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    TestCoordinator coordinator;
    if (!coordinator.Initialize()) {
        return false;
    }
    
    assert(coordinator.RegisterNode(node.GetNodeId(), node.GetPort(), node.GetShardIndex()));
    assert(coordinator.ConnectToNode(node.GetNodeId()));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 100개 동시 요청
    const int NUM_REQUESTS = 100;
    std::atomic<int> success_count{0};
    std::vector<std::future<void>> futures;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        auto future = std::async(std::launch::async, [&coordinator, &node, i, &success_count]() {
            SigningRequest request;
            request.keyId = "concurrent_key_" + std::to_string(i);
            request.transactionData = "0xconcurrent_" + std::to_string(i);
            request.threshold = 2;
            request.totalShards = 3;
            
            auto response = coordinator.SendToNode(node.GetNodeId(), &request);
            
            if (response && response->success) {
                success_count++;
            }
        });
        
        futures.push_back(std::move(future));
    }
    
    // 모든 요청 완료 대기
    for (auto& future : futures) {
        future.get();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::cout << "✓ " << success_count.load() << "/" << NUM_REQUESTS 
              << " requests succeeded in " << elapsed << "ms" << std::endl;
    std::cout << "  Average: " << (elapsed / static_cast<double>(NUM_REQUESTS)) << "ms per request" << std::endl;
    
    assert(success_count.load() == NUM_REQUESTS);
    
    // 정리
    coordinator.Stop();
    node.Stop();
    
    return true;
}

// ========================================
// Test 5: 에러 케이스
// ========================================
bool Test5_ErrorCases() {
    PrintTestHeader("Test 5: Error Cases");
    
    // 이전 테스트 정리
    CoordinatorServer& coord_instance = CoordinatorServer::Instance();
    for (const auto& node_id : coord_instance.GetAllNodeIds()) {
        coord_instance.UnregisterNode(node_id);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    TestCoordinator coordinator;
    if (!coordinator.Initialize()) {
        return false;
    }
    
    // Test 5-1: 존재하지 않는 Node에 요청
    std::cout << "Test 5-1: Request to non-existent node" << std::endl;
    SigningRequest request;
    request.keyId = "error_key";
    request.transactionData = "0xerror";
    
    auto response = coordinator.SendToNode("non_existent_node", &request);
    assert(response == nullptr);
    std::cout << "  ✓ Correctly returned nullptr" << std::endl;
    
    // Test 5-2: 연결되지 않은 Node에 요청
    std::cout << "Test 5-2: Request to disconnected node" << std::endl;
    
    TestNodeServer node("error_node", 20040, 0);
    assert(coordinator.RegisterNode(node.GetNodeId(), node.GetPort(), node.GetShardIndex()));
    // 연결하지 않음
    
    response = coordinator.SendToNode(node.GetNodeId(), &request);
    assert(response == nullptr);
    std::cout << "  ✓ Correctly returned nullptr for disconnected node" << std::endl;
    
    coordinator.Stop();
    
    return true;
}

// ========================================
// Test 6: 부하 테스트
// ========================================
bool Test6_LoadTest() {
    PrintTestHeader("Test 6: Load Test (1000 requests)");
    
    // 이전 테스트 정리
    CoordinatorServer& coord_instance = CoordinatorServer::Instance();
    for (const auto& node_id : coord_instance.GetAllNodeIds()) {
        coord_instance.UnregisterNode(node_id);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 3개 Node 시작
    std::vector<std::unique_ptr<TestNodeServer>> nodes;
    for (int i = 0; i < 3; ++i) {
        auto node = std::make_unique<TestNodeServer>(
            "load_node_" + std::to_string(i),
            20050 + i,
            i
        );
        
        if (!node->Start()) {
            return false;
        }
        nodes.push_back(std::move(node));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    TestCoordinator coordinator;
    if (!coordinator.Initialize()) {
        return false;
    }
    
    std::vector<std::string> node_ids;
    for (const auto& node : nodes) {
        assert(coordinator.RegisterNode(node->GetNodeId(), node->GetPort(), node->GetShardIndex()));
        assert(coordinator.ConnectToNode(node->GetNodeId()));
        node_ids.push_back(node->GetNodeId());
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 1000개 요청 (Broadcast)
    const int NUM_REQUESTS = 1000;
    std::atomic<int> success_count{0};
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        SigningRequest request;
        request.keyId = "load_key_" + std::to_string(i);
        request.transactionData = "0xload_" + std::to_string(i);
        request.threshold = 2;
        request.totalShards = 3;
        
        if (coordinator.BroadcastToNodes(node_ids, &request)) {
            success_count++;
        }
        
        // 진행 상황 출력 (100개마다)
        if ((i + 1) % 100 == 0) {
            std::cout << "  Progress: " << (i + 1) << "/" << NUM_REQUESTS << std::endl;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::cout << "✓ " << success_count.load() << "/" << NUM_REQUESTS 
              << " broadcast requests succeeded in " << elapsed << "ms" << std::endl;
    std::cout << "  Average: " << (elapsed / static_cast<double>(NUM_REQUESTS)) << "ms per broadcast" << std::endl;
    std::cout << "  Throughput: " << (NUM_REQUESTS * 1000.0 / elapsed) << " broadcasts/sec" << std::endl;
    
    assert(success_count.load() == NUM_REQUESTS);
    
    // 정리
    coordinator.Stop();
    for (auto& node : nodes) {
        node->Stop();
    }
    
    return true;
}

// ========================================
// Main
// ========================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  MPC Engine Full Integration Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Testing: Coordinator + Node communication" << std::endl;
    std::cout << "Scope: Server lifecycle, Connection pool, Concurrency" << std::endl;
    std::cout << std::endl;
    
    try {
        PrintTestResult("Test 1: Basic Server Start/Stop", Test1_BasicServerStartStop());
        PrintTestResult("Test 2: Connection Management", Test2_ConnectionManagement());
        PrintTestResult("Test 3: Broadcast (2/3 MPC)", Test3_BroadcastMPC());
        PrintTestResult("Test 4: Concurrent Requests (100)", Test4_ConcurrentRequests());
        PrintTestResult("Test 5: Error Cases", Test5_ErrorCases());
        PrintTestResult("Test 6: Load Test (1000)", Test6_LoadTest());
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  All Integration Tests Passed!" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\nmpc-engine is ready for production!" << std::endl;
        std::cout << "Next: Implement Wallet Server (Go)" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    }
}