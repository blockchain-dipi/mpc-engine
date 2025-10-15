// tests/integration/test_coordinator_node_tls.cpp
#include "coordinator/CoordinatorServer.hpp"
#include "node/NodeServer.hpp"
#include "common/env/EnvManager.hpp"
#include "types/BasicTypes.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include "common/resource/include/ReadOnlyResLoaderManager.hpp"
#include "proto/coordinator_node/generated/message.pb.h"
#include "proto/coordinator_node/generated/signing.pb.h"
#include "proto/coordinator_node/generated/common.pb.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <cassert>

using namespace mpc_engine;
using namespace mpc_engine::coordinator;
using namespace mpc_engine::node;
using namespace mpc_engine::env;
using namespace mpc_engine::kms;
using namespace mpc_engine::proto::coordinator_node;
using namespace mpc_engine::resource;

class TlsTestEnvironment 
{
private:
    std::vector<std::unique_ptr<NodeServer>> node_servers;
    std::unique_ptr<CoordinatorServer> coordinator_server;
    bool is_setup = false;

public:
    bool SetupEnvironment() 
    {
        std::cout << "\n=== Setting up TLS Test Environment ===" << std::endl;

        // 1. Config 로드
        if (!EnvManager::Instance().Initialize("local")) {
            std::cerr << "Failed to initialize config" << std::endl;
            return false;
        }

        // readonly 리소스 로더 초기화
        ReadOnlyResLoaderManager::Instance().Initialize(PlatformType::LOCAL);

        // 2. KMS 초기화
        std::string kms_path = Config::GetString("NODE_LOCAL_KMS_PATH");
        KMSManager::InitializeLocal(PlatformType::LOCAL, kms_path);
        std::cout << "✓ KMS initialized" << std::endl;

        // 3. TLS Global 초기화
        TlsContext::GlobalInit();
        std::cout << "✓ TLS initialized" << std::endl;

        // 4. Node 서버들 생성 및 시작
        std::vector<std::string> node_ids = Config::GetStringArray("NODE_IDS");
        std::vector<std::pair<std::string, uint16_t>> node_hosts = Config::GetNodeEndpoints("NODE_HOSTS");
        std::vector<std::string> platforms = Config::GetStringArray("NODE_PLATFORMS");
        std::vector<std::string> tls_cert_paths = Config::GetStringArray("TLS_CERT_PATHS");
        std::vector<std::string> tls_kms_nodes_coordinator_key_ids = Config::GetStringArray("TLS_KMS_NODES_COORDINATOR_KEY_IDS");

        for (size_t i = 0; i < node_ids.size(); ++i) {
            NodeConfig config;
            config.node_id = node_ids[i];
            config.bind_address = node_hosts[i].first;
            config.bind_port = node_hosts[i].second;
            config.platform_type = PlatformTypeFromString(platforms[i]);
            config.certificate_path = tls_cert_paths[i];
            config.private_key_id = tls_kms_nodes_coordinator_key_ids[i];

            auto node_server = std::make_unique<NodeServer>(config);
            
            if (!node_server->Initialize()) {
                std::cerr << "Failed to initialize node: " << config.node_id << std::endl;
                return false;
            }

            // 보안 설정
            if (auto* tcp_server = node_server->GetTcpServer()) {
                std::string trusted_ip = Config::GetString("TRUSTED_COORDINATOR_IP");
                tcp_server->SetTrustedCoordinator(trusted_ip);
                tcp_server->EnableKernelFirewall(false);
            }

            if (!node_server->Start()) {
                std::cerr << "Failed to start node: " << config.node_id << std::endl;
                return false;
            }

            node_servers.push_back(std::move(node_server));
            std::cout << "✓ Node started: " << config.node_id 
                      << " at " << config.bind_address << ":" << config.bind_port << std::endl;
        }

        // 5. Coordinator 서버 생성
        coordinator_server = std::make_unique<CoordinatorServer>();
        if (!coordinator_server->Initialize()) {
            std::cerr << "Failed to initialize coordinator" << std::endl;
            return false;
        }

        // Node 등록
        for (size_t i = 0; i < node_ids.size(); ++i) {
            if (!coordinator_server->RegisterNode(
                node_ids[i],
                PlatformTypeFromString(platforms[i]),
                node_hosts[i].first,
                node_hosts[i].second,
                i
            )) {
                std::cerr << "Failed to register node: " << node_ids[i] << std::endl;
                return false;
            }
        }

        if (!coordinator_server->Start()) {
            std::cerr << "Failed to start coordinator" << std::endl;
            return false;
        }

        std::cout << "✓ Coordinator started" << std::endl;

        // 6. 모든 Node에 연결
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        for (const auto& node_id : node_ids) {
            if (!coordinator_server->ConnectToNode(node_id)) {
                std::cerr << "Failed to connect to node: " << node_id << std::endl;
                return false;
            }
            std::cout << "✓ Connected to node: " << node_id << std::endl;
        }

        is_setup = true;
        std::cout << "✓ Environment setup complete" << std::endl;
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

std::unique_ptr<CoordinatorNodeMessage> CreateSigningRequest(
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

bool TestBasicConnection(TlsTestEnvironment& env)
{
    std::cout << "\n=== Test 1: Basic TLS Connection ===" << std::endl;

    auto* coordinator = env.GetCoordinator();
    if (!coordinator) {
        std::cerr << "Coordinator not available" << std::endl;
        return false;
    }

    size_t connected_count = coordinator->GetConnectedNodeCount();
    size_t expected_count = env.GetNodeCount();

    std::cout << "Connected nodes: " << connected_count << "/" << expected_count << std::endl;

    if (connected_count != expected_count) {
        std::cerr << "Not all nodes connected via TLS" << std::endl;
        return false;
    }

    std::vector<std::string> node_ids = coordinator->GetConnectedNodeIds();
    for (const auto& node_id : node_ids) {
        mpc_engine::ConnectionStatus status = coordinator->GetNodeStatus(node_id);
        if (status != mpc_engine::ConnectionStatus::CONNECTED) {
            std::cerr << "Node " << node_id << " not properly connected" << std::endl;
            return false;
        }
        std::cout << "✓ Node " << node_id << " connected" << std::endl;
    }

    return true;
}

bool TestSigningProtocol(TlsTestEnvironment& env)
{
    std::cout << "\n=== Test 2: Signing Protocol over TLS ===" << std::endl;

    auto* coordinator = env.GetCoordinator();
    if (!coordinator) {
        std::cerr << "Coordinator not available" << std::endl;
        return false;
    }

    // Proto 메시지 생성
    auto signing_request = CreateSigningRequest(
        "test_signing_001",
        "test_key_tls",
        "0x" + std::string(64, 'a')
    );

    std::cout << "Sending signing request: " 
              << signing_request->signing_request().header().uid() << std::endl;
    std::cout << "Transaction data: " 
              << signing_request->signing_request().transaction_data().substr(0, 20) << "..." << std::endl;

    bool success = coordinator->BroadcastToAllConnectedNodes(signing_request.get());

    if (!success) {
        std::cerr << "Failed to broadcast signing request" << std::endl;
        return false;
    }

    std::cout << "✓ Signing request broadcasted successfully" << std::endl;
    return true;
}

bool TestConcurrentRequests(TlsTestEnvironment& env)
{
    std::cout << "\n=== Test 3: Concurrent Requests over TLS ===" << std::endl;

    auto* coordinator = env.GetCoordinator();
    if (!coordinator) {
        std::cerr << "Coordinator not available" << std::endl;
        return false;
    }

    const int num_requests = 10;
    std::vector<std::future<bool>> futures;

    std::cout << "Sending " << num_requests << " concurrent requests..." << std::endl;

    for (int i = 0; i < num_requests; ++i) {
        auto future = std::async(std::launch::async, [coordinator, i]() -> bool {
            auto request = CreateSigningRequest(
                "concurrent_test_" + std::to_string(i),
                "concurrent_key_" + std::to_string(i),
                "0x" + std::string(64, 'b' + (i % 26))
            );

            return coordinator->BroadcastToAllConnectedNodes(request.get());
        });

        futures.push_back(std::move(future));
    }

    int successful_requests = 0;
    for (auto& future : futures) {
        try {
            if (future.get()) {
                successful_requests++;
            }
        } catch (const std::exception& e) {
            std::cerr << "Request failed: " << e.what() << std::endl;
        }
    }

    std::cout << "Successful requests: " << successful_requests << "/" << num_requests << std::endl;

    if (successful_requests == num_requests) {
        std::cout << "✓ All concurrent requests successful" << std::endl;
        return true;
    } else {
        std::cerr << "Some concurrent requests failed" << std::endl;
        return false;
    }
}

bool TestStressTest(TlsTestEnvironment& env)
{
    std::cout << "\n=== Test 4: TLS Stress Test (50 requests) ===" << std::endl;

    auto* coordinator = env.GetCoordinator();
    if (!coordinator) {
        std::cerr << "Coordinator not available" << std::endl;
        return false;
    }

    const int stress_requests = 50;
    int successful = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < stress_requests; ++i) {
        auto request = CreateSigningRequest(
            "stress_test_" + std::to_string(i),
            "stress_key_" + std::to_string(i),
            "0x" + std::string(64, 'c' + (i % 26))
        );

        if (coordinator->BroadcastToAllConnectedNodes(request.get())) {
            successful++;
        }

        if (i % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Stress test results:" << std::endl;
    std::cout << "  Successful: " << successful << "/" << stress_requests << std::endl;
    std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "  Rate: " << (successful * 1000.0) / duration.count() << " req/sec" << std::endl;

    if (successful >= stress_requests * 0.95) {
        std::cout << "✓ Stress test passed" << std::endl;
        return true;
    } else {
        std::cerr << "Stress test failed (< 95% success rate)" << std::endl;
        return false;
    }
}

void PrintTestResult(const std::string& test_name, bool result)
{
    std::cout << "[" << (result ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

int main()
{
    std::cout << "================================================" << std::endl;
    std::cout << "  MPC Engine TLS Integration Test (Protobuf)" << std::endl;
    std::cout << "================================================" << std::endl;

    TlsTestEnvironment env;

    try {
        if (!env.SetupEnvironment()) {
            std::cerr << "Failed to setup test environment" << std::endl;
            return 1;
        }

        std::cout << "\nWaiting for connections to stabilize..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        bool test1 = TestBasicConnection(env);
        bool test2 = TestSigningProtocol(env);
        bool test3 = TestConcurrentRequests(env);
        bool test4 = TestStressTest(env);

        std::cout << "\n=== Test Results ===" << std::endl;
        PrintTestResult("Basic TLS Connection", test1);
        PrintTestResult("Signing Protocol over TLS", test2);
        PrintTestResult("Concurrent Requests", test3);
        PrintTestResult("TLS Stress Test", test4);

        bool all_passed = test1 && test2 && test3 && test4;
        
        std::cout << "\n================================================" << std::endl;
        if (all_passed) {
            std::cout << "  🎉 ALL TESTS PASSED!" << std::endl;
            std::cout << "  TLS-based MPC system with Protobuf is working" << std::endl;
        } else {
            std::cout << "  ❌ SOME TESTS FAILED" << std::endl;
            std::cout << "  Please check the implementation" << std::endl;
        }
        std::cout << "================================================" << std::endl;

        env.TeardownEnvironment();

        return all_passed ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "Test exception: " << e.what() << std::endl;
        env.TeardownEnvironment();
        return 1;
    }
}