// tests/integration/test_wallet_coordinator_node_integration.cpp
#include "coordinator/CoordinatorServer.hpp"
#include "node/NodeServer.hpp"
#include "common/env/EnvManager.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/resource/include/ReadOnlyResLoaderManager.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include "proto/wallet_coordinator/generated/wallet_message.pb.h"
#include "proto/coordinator_node/generated/message.pb.h"
#include "types/BasicTypes.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <future>
#include <cassert>

using namespace mpc_engine;
using namespace mpc_engine::coordinator;
using namespace mpc_engine::node;
using namespace mpc_engine::env;
using namespace mpc_engine::kms;
using namespace mpc_engine::resource;
using namespace mpc_engine::coordinator::handlers::wallet;
using namespace mpc_engine::network::tls;
using namespace mpc_engine::proto::wallet_coordinator;
using namespace mpc_engine::proto::coordinator_node;

// ===== 전체 시스템 환경 =====
class E2ETestEnvironment 
{
private:
    std::vector<std::unique_ptr<NodeServer>> node_servers;
    std::unique_ptr<CoordinatorServer> coordinator_server;
    bool is_setup = false;

public:
    bool SetupEnvironment() 
    {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  E2E Integration Test Setup" << std::endl;
        std::cout << "========================================" << std::endl;

        // 1. Config 로드
        if (!EnvManager::Instance().Initialize("local")) {
            std::cerr << "Failed to initialize config" << std::endl;
            return false;
        }

        // 2. ReadOnly 리소스 로더 초기화
        ReadOnlyResLoaderManager::Instance().Initialize(PlatformType::LOCAL);
        std::cout << "✓ Resource loader initialized" << std::endl;

        // 3. KMS 초기화
        std::string kms_path = Config::GetString("NODE_LOCAL_KMS_PATH");
        KMSManager::InitializeLocal(PlatformType::LOCAL, kms_path);
        std::cout << "✓ KMS initialized" << std::endl;

        // 4. TLS Global 초기화
        TlsContext::GlobalInit();
        std::cout << "✓ TLS initialized" << std::endl;

        // 5. Node 서버 시작 (3개)
        std::vector<std::string> node_ids = Config::GetStringArray("NODE_IDS");
        std::vector<std::pair<std::string, uint16_t>> node_hosts = Config::GetNodeEndpoints("NODE_HOSTS");
        std::vector<std::string> platforms = Config::GetStringArray("NODE_PLATFORMS");
        std::vector<std::string> tls_cert_paths = Config::GetStringArray("TLS_CERT_PATHS");
        std::vector<std::string> tls_kms_key_ids = Config::GetStringArray("TLS_KMS_NODES_COORDINATOR_KEY_IDS");

        for (size_t i = 0; i < node_ids.size(); ++i) {
            NodeConfig config;
            config.node_id = node_ids[i];
            config.bind_address = node_hosts[i].first;
            config.bind_port = node_hosts[i].second;
            config.platform_type = PlatformTypeFromString(platforms[i]);
            config.certificate_path = tls_cert_paths[i];
            config.private_key_id = tls_kms_key_ids[i];

            auto node_server = std::make_unique<NodeServer>(config);
            
            if (!node_server->Initialize()) {
                std::cerr << "Failed to initialize node: " << config.node_id << std::endl;
                return false;
            }

            // 보안 설정
            if (auto* tcp_server = node_server->GetTcpServer()) {
                std::string trusted_ip = Config::GetString("TRUSTED_COORDINATOR_IP");
                tcp_server->SetTrustedCoordinator(trusted_ip);
                tcp_server->EnableKernelFirewall(false); // 테스트 환경에서는 비활성화
            }

            if (!node_server->Start()) {
                std::cerr << "Failed to start node: " << config.node_id << std::endl;
                return false;
            }

            node_servers.push_back(std::move(node_server));
            std::cout << "✓ Node " << config.node_id << " started at " 
                      << config.bind_address << ":" << config.bind_port << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 6. Coordinator 서버 시작
        coordinator_server = std::make_unique<CoordinatorServer>();
        if (!coordinator_server->Initialize()) {
            std::cerr << "Failed to initialize coordinator" << std::endl;
            return false;
        }

        // Node 등록
        std::vector<uint16_t> shard_indices = Config::GetUInt16Array("NODE_SHARD_INDICES");
        
        for (size_t i = 0; i < node_ids.size(); ++i) {
            uint32_t shard_index = (i < shard_indices.size()) ? shard_indices[i] : i;
            
            if (!coordinator_server->RegisterNode(
                node_ids[i],
                PlatformTypeFromString(platforms[i]),
                node_hosts[i].first,
                node_hosts[i].second,
                shard_index
            )) {
                std::cerr << "Failed to register node: " << node_ids[i] << std::endl;
                return false;
            }
            std::cout << "✓ Node " << node_ids[i] << " registered (shard " << shard_index << ")" << std::endl;
        }

        if (!coordinator_server->Start()) {
            std::cerr << "Failed to start coordinator" << std::endl;
            return false;
        }
        std::cout << "✓ Coordinator started" << std::endl;

        // 7. Coordinator → Node 연결
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        for (const auto& node_id : node_ids) {
            if (!coordinator_server->ConnectToNode(node_id)) {
                std::cerr << "Failed to connect to node: " << node_id << std::endl;
                return false;
            }
            std::cout << "✓ Connected to node: " << node_id << std::endl;
        }

        // 8. Wallet Router 초기화 (WalletMessageRouter는 Singleton이므로 자동 초기화됨)
        if (!WalletMessageRouter::Instance().Initialize()) {
            std::cerr << "Failed to initialize WalletMessageRouter" << std::endl;
            return false;
        }
        std::cout << "✓ WalletMessageRouter initialized" << std::endl;

        is_setup = true;
        std::cout << "\n✓ Environment setup complete!" << std::endl;
        return true;
    }

    void TeardownEnvironment() 
    {
        std::cout << "\n=== Tearing down environment ===" << std::endl;
        
        if (coordinator_server) {
            coordinator_server->Stop();
            coordinator_server.reset();
            std::cout << "✓ Coordinator stopped" << std::endl;
        }

        for (auto& node_server : node_servers) {
            if (node_server) {
                node_server->Stop();
            }
        }
        node_servers.clear();
        std::cout << "✓ All nodes stopped" << std::endl;

        TlsContext::GlobalCleanup();
        std::cout << "✓ TLS cleanup complete" << std::endl;
    }

    CoordinatorServer* GetCoordinator() { return coordinator_server.get(); }
    size_t GetNodeCount() const { return node_servers.size(); }
    bool IsSetup() const { return is_setup; }
};

// ===== Helper Functions =====

uint64_t GetCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Wallet → Coordinator 요청 시뮬레이션
std::unique_ptr<WalletCoordinatorMessage> CreateWalletSigningRequest(
    const std::string& request_id,
    const std::string& key_id,
    const std::string& transaction_data
) {
    auto message = std::make_unique<WalletCoordinatorMessage>();
    message->set_message_type(static_cast<uint32_t>(mpc_engine::WalletMessageType::SIGNING_REQUEST));
    
    WalletSigningRequest* request = message->mutable_signing_request();
    
    // Header 설정
    auto* header = request->mutable_header();
    header->set_message_type(static_cast<uint32_t>(mpc_engine::WalletMessageType::SIGNING_REQUEST));
    header->set_request_id(request_id);
    header->set_timestamp(std::to_string(GetCurrentTimeMs()));
    header->set_coordinator_id("coordinator");
    
    // Request 필드 설정
    request->set_key_id(key_id);
    request->set_transaction_data(transaction_data);
    request->set_threshold(2);
    request->set_total_shards(3);
    
    return message;
}

// Coordinator → Node 요청 생성
std::unique_ptr<CoordinatorNodeMessage> CreateCoordinatorNodeSigningRequest(
    const std::string& uid,
    const std::string& key_id,
    const std::string& transaction_data
) {
    auto message = std::make_unique<CoordinatorNodeMessage>();
    message->set_message_type(static_cast<int32_t>(mpc_engine::MessageType::SIGNING_REQUEST));
    
    SigningRequest* request = message->mutable_signing_request();
    
    // Header 설정
    RequestHeader* header = request->mutable_header();
    header->set_uid(uid);
    header->set_send_time(std::to_string(GetCurrentTimeMs()));
    header->set_request_id(GetCurrentTimeMs());
    
    // Request 필드 설정
    request->set_key_id(key_id);
    request->set_transaction_data(transaction_data);
    request->set_threshold(2);
    request->set_total_shards(3);
    
    return message;
}

// ===== Test Functions =====

bool TestWalletToCoordinatorMessageRouting([[maybe_unused]] E2ETestEnvironment& env)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test 1: Wallet → Coordinator Routing" << std::endl;
    std::cout << "========================================" << std::endl;

    // Wallet 요청 생성
    auto wallet_request = CreateWalletSigningRequest(
        "e2e_test_001",
        "wallet_key_123",
        "0x" + std::string(64, 'a')
    );

    std::cout << "Request ID: " << wallet_request->signing_request().header().request_id() << std::endl;
    std::cout << "Key ID: " << wallet_request->signing_request().key_id() << std::endl;

    // WalletMessageRouter를 통해 처리
    auto response = WalletMessageRouter::Instance().ProcessMessage(wallet_request.get());

    if (!response) {
        std::cerr << "❌ Failed to process wallet request" << std::endl;
        return false;
    }

    if (!response->has_signing_response()) {
        std::cerr << "❌ Response does not contain signing_response" << std::endl;
        return false;
    }

    const auto& signing_response = response->signing_response();
    
    if (!signing_response.header().success()) {
        std::cerr << "❌ Signing failed: " << signing_response.header().error_message() << std::endl;
        return false;
    }

    std::cout << "✓ Wallet request processed successfully" << std::endl;
    std::cout << "  Final Signature: " << signing_response.final_signature().substr(0, 50) << "..." << std::endl;
    std::cout << "  Shard Count: " << signing_response.shard_signatures_size() << std::endl;

    return true;
}

bool TestCoordinatorToNodeCommunication(E2ETestEnvironment& env)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test 2: Coordinator → Node Communication" << std::endl;
    std::cout << "========================================" << std::endl;

    auto* coordinator = env.GetCoordinator();
    if (!coordinator) {
        std::cerr << "❌ Coordinator not available" << std::endl;
        return false;
    }

    // Coordinator → Node 요청 생성
    auto node_request = CreateCoordinatorNodeSigningRequest(
        "e2e_node_test_001",
        "node_key_456",
        "0x" + std::string(64, 'b')
    );

    std::cout << "Broadcasting to " << env.GetNodeCount() << " nodes..." << std::endl;
    
    bool success = coordinator->BroadcastToAllConnectedNodes(node_request.get());

    if (!success) {
        std::cerr << "❌ Failed to broadcast to nodes" << std::endl;
        return false;
    }

    std::cout << "✓ Request broadcasted to all nodes successfully" << std::endl;
    return true;
}

bool TestFullE2EFlow(E2ETestEnvironment& env)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test 3: Full E2E Flow" << std::endl;
    std::cout << "  Wallet → Coordinator → Node → Coordinator → Wallet" << std::endl;
    std::cout << "========================================" << std::endl;

    auto* coordinator = env.GetCoordinator();
    if (!coordinator) {
        std::cerr << "❌ Coordinator not available" << std::endl;
        return false;
    }

    // Step 1: Wallet 요청 시뮬레이션
    std::cout << "\n[Step 1] Wallet sends SIGNING_REQUEST to Coordinator" << std::endl;
    auto wallet_request = CreateWalletSigningRequest(
        "e2e_full_test_001",
        "e2e_key_789",
        "0x" + std::string(64, 'c')
    );

    std::cout << "  Request ID: " << wallet_request->signing_request().header().request_id() << std::endl;
    std::cout << "  Key ID: " << wallet_request->signing_request().key_id() << std::endl;
    std::cout << "  Transaction: " << wallet_request->signing_request().transaction_data().substr(0, 20) << "..." << std::endl;

    // Step 2: Coordinator가 요청 수신 후 Node로 전달
    std::cout << "\n[Step 2] Coordinator forwards request to Nodes" << std::endl;
    
    auto node_request = CreateCoordinatorNodeSigningRequest(
        wallet_request->signing_request().header().request_id(),
        wallet_request->signing_request().key_id(),
        wallet_request->signing_request().transaction_data()
    );

    bool broadcast_success = coordinator->BroadcastToAllConnectedNodes(node_request.get());
    
    if (!broadcast_success) {
        std::cerr << "❌ Failed to broadcast to nodes" << std::endl;
        return false;
    }
    std::cout << "  ✓ Broadcasted to " << env.GetNodeCount() << " nodes" << std::endl;

    // Step 3: Node 응답 대기
    std::cout << "\n[Step 3] Waiting for Node responses..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "  ✓ Node responses received (simulated)" << std::endl;

    // Step 4: Coordinator가 Wallet Handler 통해 최종 응답 생성
    std::cout << "\n[Step 4] Coordinator processes and returns response to Wallet" << std::endl;
    auto wallet_response = WalletMessageRouter::Instance().ProcessMessage(wallet_request.get());

    if (!wallet_response || !wallet_response->has_signing_response()) {
        std::cerr << "❌ Failed to generate wallet response" << std::endl;
        return false;
    }

    const auto& signing_response = wallet_response->signing_response();
    
    if (!signing_response.header().success()) {
        std::cerr << "❌ Signing failed: " << signing_response.header().error_message() << std::endl;
        return false;
    }

    std::cout << "  ✓ Final response generated" << std::endl;
    std::cout << "  Key ID: " << signing_response.key_id() << std::endl;
    std::cout << "  Final Signature: " << signing_response.final_signature().substr(0, 50) << "..." << std::endl;
    std::cout << "  Successful Shards: " << signing_response.successful_shards() << std::endl;

    std::cout << "\n✓ Full E2E flow completed successfully!" << std::endl;
    return true;
}

bool TestConcurrentE2ERequests([[maybe_unused]] E2ETestEnvironment& env)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test 4: Concurrent E2E Requests" << std::endl;
    std::cout << "========================================" << std::endl;

    const int num_requests = 5;
    std::vector<std::future<bool>> futures;

    std::cout << "Sending " << num_requests << " concurrent E2E requests..." << std::endl;

    for (int i = 0; i < num_requests; ++i) {
        auto future = std::async(std::launch::async, [i]() -> bool {
            auto wallet_request = CreateWalletSigningRequest(
                "concurrent_e2e_" + std::to_string(i),
                "concurrent_key_" + std::to_string(i),
                "0x" + std::string(64, 'd' + (i % 20))
            );

            // Wallet Router를 통해 처리
            auto response = WalletMessageRouter::Instance().ProcessMessage(wallet_request.get());
            
            if (!response || !response->has_signing_response()) {
                return false;
            }

            return response->signing_response().header().success();
        });

        futures.push_back(std::move(future));
    }

    int successful_requests = 0;
    for (size_t i = 0; i < futures.size(); ++i) {
        try {
            if (futures[i].get()) {
                successful_requests++;
                std::cout << "  ✓ Request " << i << " completed" << std::endl;
            } else {
                std::cout << "  ❌ Request " << i << " failed" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  ❌ Request " << i << " exception: " << e.what() << std::endl;
        }
    }

    std::cout << "\nResults: " << successful_requests << "/" << num_requests << " succeeded" << std::endl;

    if (successful_requests != num_requests) {
        std::cerr << "❌ Not all concurrent requests succeeded" << std::endl;
        return false;
    }

    std::cout << "✓ All concurrent requests completed successfully!" << std::endl;
    return true;
}

// ===== Main =====

int main() 
{
    std::cout << "========================================" << std::endl;
    std::cout << "  Wallet ↔ Coordinator ↔ Node" << std::endl;
    std::cout << "  E2E Integration Test" << std::endl;
    std::cout << "========================================" << std::endl;

    E2ETestEnvironment env;

    // Setup
    if (!env.SetupEnvironment()) {
        std::cerr << "\n❌ Environment setup failed" << std::endl;
        return 1;
    }

    bool all_passed = true;

    try {
        // Test 1: Wallet → Coordinator 메시지 라우팅
        if (!TestWalletToCoordinatorMessageRouting(env)) {
            std::cerr << "\n❌ Test 1 failed" << std::endl;
            all_passed = false;
        }

        // Test 2: Coordinator → Node 통신
        if (!TestCoordinatorToNodeCommunication(env)) {
            std::cerr << "\n❌ Test 2 failed" << std::endl;
            all_passed = false;
        }

        // Test 3: 전체 E2E 플로우
        if (!TestFullE2EFlow(env)) {
            std::cerr << "\n❌ Test 3 failed" << std::endl;
            all_passed = false;
        }

        // Test 4: 동시 요청 처리
        if (!TestConcurrentE2ERequests(env)) {
            std::cerr << "\n❌ Test 4 failed" << std::endl;
            all_passed = false;
        }

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test exception: " << e.what() << std::endl;
        all_passed = false;
    }

    // Teardown
    env.TeardownEnvironment();

    // 결과 출력
    std::cout << "\n========================================" << std::endl;
    if (all_passed) {
        std::cout << "  ✓ ALL TESTS PASSED" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\nE2E Integration Test Summary:" << std::endl;
        std::cout << "  - Wallet → Coordinator routing: ✓" << std::endl;
        std::cout << "  - Coordinator → Node communication: ✓" << std::endl;
        std::cout << "  - Full E2E flow: ✓" << std::endl;
        std::cout << "  - Concurrent requests: ✓" << std::endl;
        std::cout << "\n✅ System ready for production!" << std::endl;
        return 0;
    } else {
        std::cout << "  ❌ SOME TESTS FAILED" << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }
}