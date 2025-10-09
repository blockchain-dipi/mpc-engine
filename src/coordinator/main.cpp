// src/coordinator/main.cpp
#include "CoordinatorServer.hpp"
#include "common/config/ConfigManager.hpp"
#include "common/types/BasicTypes.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/kms/include/KMSException.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "protocols/coordinator_node/include/SigningProtocol.hpp"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>

using namespace mpc_engine::coordinator;
using namespace mpc_engine::node;
using namespace mpc_engine::config;
using namespace mpc_engine::kms;
using namespace mpc_engine::network::tls;
using namespace mpc_engine::protocol::coordinator_node;

// 전역 상태 관리
static CoordinatorServer* g_coordinator = nullptr;
static std::atomic<bool> g_shutdown_requested{false};
static std::condition_variable g_shutdown_cv;
static std::mutex g_shutdown_mutex;

void SignalHandler(int signal) 
{
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    
    if (g_coordinator) {
        g_coordinator->Stop();
    }
    
    {
        std::lock_guard<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_requested.store(true);
    }
    g_shutdown_cv.notify_one();
}

void PrintUsage(const char* program_name) 
{
    std::cout << "Usage: " << program_name << " [ENVIRONMENT]" << std::endl;
    std::cout << "       " << program_name << " --env [ENVIRONMENT]" << std::endl;
    std::cout << std::endl;
    std::cout << "Environment:" << std::endl;
    std::cout << "  local       Local development environment (default)" << std::endl;
    std::cout << "  dev         Development environment" << std::endl;
    std::cout << "  qa          QA environment" << std::endl;
    std::cout << "  production  Production environment" << std::endl;
}

int main(int argc, char* argv[]) 
{
    std::cout << "=== MPC Engine Coordinator Server ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << std::endl;
    
    // 명령행 인자 파싱
    std::string env_type = "local";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--env" && i + 1 < argc) {
            env_type = argv[++i];
        } else {
            env_type = arg;
            break;
        }
    }
    
    std::cout << "Loading environment: " << env_type << std::endl;
    
    // 환경 설정 로드
    if (!ConfigManager::Instance().Initialize(env_type)) {
        std::cerr << "Failed to load environment: " << env_type << std::endl;
        return 1;
    }
    
    try {
        std::vector<std::string> required_keys = {
            "COORDINATOR_PLATFORM",
            "NODE_HOSTS",
            "NODE_IDS",
            "NODE_PLATFORMS",
            "NODE_SHARD_INDICES",
            "MPC_THRESHOLD",
            "MPC_TOTAL_SHARDS",
            "WALLET_SERVER_URL",
            "WALLET_SERVER_AUTH_TOKEN"
        };        
        std::cout << "Validating required configuration..." << std::endl;
        Config::ValidateRequired(required_keys);
        std::cout << "✓ All required configurations present" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Configuration error: " << e.what() << std::endl;
        std::cerr << "\nPlease check your env/.env." << env_type << " file." << std::endl;
        return 1;
    }
    
    try {
        // ========================================
        // 1. KMS 초기화
        // ========================================
        std::string platform_type = Config::GetString("COORDINATOR_PLATFORM");
        NodePlatformType platform = FromString(platform_type);
        std::cout << "\n=== KMS Initialization ===" << std::endl;
        std::cout << "Coordinator Platform: " << platform_type << std::endl;
        
        std::string kms_config_path;
        if (platform == NodePlatformType::LOCAL) {
            kms_config_path = Config::GetString("COORDINATOR_LOCAL_KMS_PATH");
        }
        KMSManager::InitializeLocal(NodePlatformType::UNKNOWN, kms_config_path);
        std::cout << "✓ KMS initialized successfully" << std::endl;
        
        // ========================================
        // 2. Coordinator Server 초기화
        // ========================================
        std::cout << "\n=== Coordinator Server Initialization ===" << std::endl;
        
        CoordinatorServer& coordinator = CoordinatorServer::Instance();
        g_coordinator = &coordinator;
        
        // 시그널 핸들러 등록
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);
        
        if (!coordinator.Initialize()) {
            std::cerr << "Failed to initialize coordinator" << std::endl;
            return 1;
        }
        
        if (!coordinator.Start()) {
            std::cerr << "Failed to start coordinator" << std::endl;
            return 1;
        }
        std::cout << "✓ Coordinator server started" << std::endl;
        
        // ========================================
        // 3. Wallet Server 초기화
        // ========================================
        std::cout << "\n=== Wallet Server Initialization ===" << std::endl;

        std::string wallet_url = Config::GetString("WALLET_SERVER_URL");
        std::string wallet_auth_token = Config::GetString("WALLET_SERVER_AUTH_TOKEN");

        if (!coordinator.InitializeWalletServer(wallet_url, wallet_auth_token)) {
            std::cerr << "✗ Failed to initialize Wallet Server" << std::endl;
            std::cerr << "  Note: This is expected if Wallet Server is not running yet" << std::endl;
        } else {
            std::cout << "✓ Wallet Server initialized" << std::endl;
        }
        
        // ========================================
        // 4. Node 설정 로드 및 등록
        // ========================================
        std::cout << "\n=== Node Configuration ===" << std::endl;
        
        std::vector<std::pair<std::string, uint16_t>> node_endpoints = Config::GetNodeEndpoints("NODE_HOSTS");
        std::vector<std::string> node_ids = Config::GetStringArray("NODE_IDS");
        std::vector<std::string> platforms = Config::GetStringArray("NODE_PLATFORMS");
        std::vector<uint16_t> shard_indices = Config::GetUInt16Array("NODE_SHARD_INDICES");
        uint32_t threshold = Config::GetUInt32("MPC_THRESHOLD");
        uint32_t total_shards = Config::GetUInt32("MPC_TOTAL_SHARDS");
        
        if (node_endpoints.empty()) {
            std::cerr << "No node endpoints configured" << std::endl;
            return 1;
        }
        
        std::cout << "  Environment: " << env_type << std::endl;
        std::cout << "  Platform: " << platform_type << std::endl;
        std::cout << "  MPC Threshold: " << threshold << "/" << total_shards << std::endl;
        std::cout << "  Target Nodes:" << std::endl;
        
        for (size_t i = 0; i < node_endpoints.size(); ++i) {
            const auto& endpoint = node_endpoints[i];
            std::string node_id = (i < node_ids.size()) ? node_ids[i] : "node_" + std::to_string(i + 1);
            std::string node_platform = (i < platforms.size()) ? platforms[i] : "LOCAL";
            uint32_t shard_index = (i < shard_indices.size()) ? shard_indices[i] : i;
            
            std::cout << "    - " << node_id << " (" << node_platform << ") at " 
                      << endpoint.first << ":" << endpoint.second 
                      << " [shard " << shard_index << "]" << std::endl;
            
            NodePlatformType platform_type = FromString(node_platform);
            
            if (!coordinator.RegisterNode(node_id, platform_type, endpoint.first, endpoint.second, shard_index)) {
                std::cerr << "Failed to register node: " << node_id << std::endl;
                return 1;
            }
        }
        std::cout << "✓ All nodes registered" << std::endl;
        
        // ========================================
        // 5. Node 연결
        // ========================================
        std::cout << "\n=== Node Connection ===" << std::endl;
        std::cout << "Attempting to connect to registered nodes..." << std::endl;
        
        for (const auto& node_id : node_ids) {
            if (coordinator.ConnectToNode(node_id)) {
                std::cout << "  ✓ Connected to " << node_id << std::endl;
            } else {
                std::cout << "  ✗ Failed to connect to " << node_id 
                          << " (Node may not be running yet)" << std::endl;
            }
        }
        
        size_t connected_count = coordinator.GetConnectedNodeCount();
        std::cout << "\nConnected nodes: " << connected_count << "/" << node_ids.size() << std::endl;
        
        if (connected_count == 0) {
            std::cout << "\nWarning: No nodes connected" << std::endl;
            std::cout << "  Coordinator is running, but cannot process MPC operations" << std::endl;
            std::cout << "  Start Node servers and they will auto-connect" << std::endl;
        }
        
        // ========================================
        // 6. 시작 정보 출력
        // ========================================
        std::cout << "\n========================================" << std::endl;
        std::cout << "  Coordinator Server Running" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Environment: " << env_type << std::endl;
        std::cout << "  Platform: " << platform_type << std::endl;
        std::cout << "  MPC Threshold: " << threshold << "/" << total_shards << std::endl;
        std::cout << "  Registered Nodes: " << node_ids.size() << std::endl;
        std::cout << "  Connected Nodes: " << connected_count << std::endl;
        std::cout << "  Wallet Server: " << (coordinator.IsWalletServerInitialized() ? "✓ Ready" : "✗ Not ready") << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\nPress Ctrl+C to shutdown gracefully..." << std::endl;
        
        // ========================================
        // 7. 메인 루프
        // ========================================
        {
            std::unique_lock<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.wait(lock, [] { return g_shutdown_requested.load(); });
        }
        
        std::cout << "\nShutdown initiated..." << std::endl;
        coordinator.Stop();
        
        std::cout << "Coordinator server stopped cleanly" << std::endl;
        return 0;
        
    } catch (const ConfigMissingException& e) {
        std::cerr << "\n✗ Configuration Error: " << e.what() << std::endl;
        std::cerr << "Check your env/.env." << env_type << " file" << std::endl;
        return 1;
    } catch (const KMSException& e) {
        std::cerr << "\n✗ KMS Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Fatal Error: " << e.what() << std::endl;
        return 1;
    }
}