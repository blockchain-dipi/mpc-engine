// src/coordinator/main.cpp
#include "CoordinatorServer.hpp"
#include "common/types/BasicTypes.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/config/EnvConfig.hpp"
#include "common/kms/include/KMSFactory.hpp"
#include "common/kms/include/KMSException.hpp"
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

void ValidateCoordinatorConfig(const EnvConfig& config) {
    std::vector<std::string> required_keys = {
        "COORDINATOR_PLATFORM",
        "NODE_HOSTS",
        "NODE_IDS",
        "NODE_PLATFORMS",
        "NODE_SHARD_INDICES",
        "MPC_THRESHOLD",
        "MPC_TOTAL_SHARDS"
    };
    
    std::cout << "Validating required configuration..." << std::endl;
    config.ValidateRequired(required_keys);
    std::cout << "✓ All required configurations present" << std::endl;
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
    EnvConfig env_config;
    if (!env_config.LoadFromEnv(env_type)) {
        std::cerr << "Failed to load environment configuration: " << env_type << std::endl;
        return 1;
    }
    
    try {
        ValidateCoordinatorConfig(env_config);
    } catch (const std::exception& e) {
        std::cerr << "✗ Configuration error: " << e.what() << std::endl;
        std::cerr << "\nPlease check your env/.env." << env_type << " file." << std::endl;
        return 1;
    }
    
    try {
        // 🆕 KMS 초기화
        std::string platform = env_config.GetString("COORDINATOR_PLATFORM");
        std::cout << "\n=== KMS Initialization ===" << std::endl;
        std::cout << "Coordinator Platform: " << platform << std::endl;
        
        std::string kms_config_path;
        if (platform == "LOCAL" || platform == "local") {
            kms_config_path = env_config.GetString("COORDINATOR_LOCAL_KMS_PATH");
        }
        // 나중에 AWS, Azure 등 추가
        
        auto kms = KMSFactory::Create(platform, kms_config_path);
        if (!kms->Initialize()) {
            std::cerr << "Failed to initialize KMS" << std::endl;
            return 1;
        }
        std::cout << "✓ KMS initialized successfully" << std::endl;
        std::cout << std::endl;
        
        // 기존 Coordinator 초기화
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
        
        // Config 로드
        std::vector<std::pair<std::string, uint16_t>> node_endpoints = env_config.GetNodeEndpoints("NODE_HOSTS");
        std::vector<std::string> node_ids = env_config.GetStringArray("NODE_IDS");
        std::vector<std::string> platforms = env_config.GetStringArray("NODE_PLATFORMS");
        std::vector<uint16_t> shard_indices = env_config.GetUInt16Array("NODE_SHARD_INDICES");
        uint32_t threshold = env_config.GetUInt32("MPC_THRESHOLD");
        uint32_t total_shards = env_config.GetUInt32("MPC_TOTAL_SHARDS");
        
        if (node_endpoints.empty()) {
            std::cerr << "No node endpoints configured" << std::endl;
            return 1;
        }
        
        std::cout << "\nCoordinator configuration:" << std::endl;
        std::cout << "  Environment: " << env_type << std::endl;
        std::cout << "  Platform: " << platform << std::endl;
        std::cout << "  MPC Threshold: " << threshold << "/" << total_shards << std::endl;
        std::cout << "  Target Nodes:" << std::endl;
        for (size_t i = 0; i < node_endpoints.size(); ++i) {
            const auto& endpoint = node_endpoints[i];
            std::string node_id = (i < node_ids.size()) ? node_ids[i] : "node_" + std::to_string(i + 1);
            std::string node_platform = (i < platforms.size()) ? platforms[i] : "UNKNOWN";
            std::cout << "    " << node_id << " (" << node_platform << ") - " 
                      << endpoint.first << ":" << endpoint.second << std::endl;
        }
        std::cout << std::endl;
        
        // Node 등록
        std::cout << "Registering nodes..." << std::endl;
        for (size_t i = 0; i < node_endpoints.size(); ++i) {
            const auto& endpoint = node_endpoints[i];
            
            std::string node_id = (i < node_ids.size()) ? node_ids[i] : "node_" + std::to_string(i + 1);
            NodePlatformType platform_type = (i < platforms.size()) ? FromString(platforms[i]) : NodePlatformType::LOCAL;
            uint32_t shard_index = (i < shard_indices.size()) ? shard_indices[i] : static_cast<uint32_t>(i);
            
            if (coordinator.RegisterNode(node_id, platform_type, endpoint.first, endpoint.second, shard_index)) {
                std::cout << "  ✓ " << node_id << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Node 연결
        std::cout << "Connecting to nodes..." << std::endl;
        int connected_count = 0;
        for (const auto& node_id : coordinator.GetAllNodeIds()) {
            if (coordinator.ConnectToNode(node_id)) {
                std::cout << "  ✓ " << node_id << std::endl;
                connected_count++;
            } else {
                std::cout << "  ✗ " << node_id << " (connection failed)" << std::endl;
            }
        }
        std::cout << std::endl;
        
        // 시작 정보 출력
        std::cout << "========================================" << std::endl;
        std::cout << "  Coordinator Server Running" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Platform: " << platform << std::endl;
        std::cout << "  Connected Nodes: " << connected_count << "/" << node_endpoints.size() << std::endl;
        std::cout << "  MPC Threshold: " << threshold << "/" << total_shards << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
        std::cout << std::endl;

        // 메인 루프
        {
            std::unique_lock<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.wait(lock, []{ 
                return g_shutdown_requested.load() || !g_coordinator->IsRunning(); 
            });
        }
        
    } catch (const ConfigMissingException& e) {
        std::cerr << "✗ Configuration error: " << e.what() << std::endl;
        return 1;
    } catch (const KMSException& e) {
        std::cerr << "✗ KMS error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Coordinator error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Coordinator shutdown complete." << std::endl;
    return 0;
}