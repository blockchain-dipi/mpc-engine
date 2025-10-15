// src/coordinator/main.cpp
#include "CoordinatorServer.hpp"
#include "types/BasicTypes.hpp"
#include "common/env/EnvManager.hpp"
#include "common/kms/include/KMSException.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/resource/include/ReadOnlyResLoaderManager.hpp"
#include "common/utils/logger/Logger.hpp"
#include <signal.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

using namespace mpc_engine;
using namespace mpc_engine::coordinator;
using namespace mpc_engine::env;
using namespace mpc_engine::kms;
using namespace mpc_engine::network::tls;
using namespace mpc_engine::resource;

// 전역 상태 관리
static CoordinatorServer* g_coordinator = nullptr;
static std::atomic<bool> g_shutdown_requested{false};
static std::condition_variable g_shutdown_cv;
static std::mutex g_shutdown_mutex;

void SignalHandler(int signal) 
{
    LOG_INFOF("CoordinatorServer", "Received signal %d, shutting down gracefully...", signal);
    
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
    LOG_INFOF("CoordinatorServer", "Usage: %s [ENVIRONMENT]", program_name);
    LOG_INFOF("CoordinatorServer", "       %s --env [ENVIRONMENT]", program_name);
    LOG_INFO("CoordinatorServer", "");
    LOG_INFOF("CoordinatorServer", "Environment:");
    LOG_INFOF("CoordinatorServer", "  local       Local development environment (default)");
    LOG_INFOF("CoordinatorServer", "  dev         Development environment");
    LOG_INFOF("CoordinatorServer", "  qa          QA environment");
    LOG_INFOF("CoordinatorServer", "  production  Production environment");
}

int main(int argc, char* argv[]) 
{
    LOG_INFO("CoordinatorServer", "=== MPC Engine Coordinator Server ===");
    LOG_INFOF("CoordinatorServer", "Build: %s %s", __DATE__, __TIME__);
    
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

    LOG_INFOF("CoordinatorServer", "Loading environment: %s", env_type.c_str());

    // 환경 설정 로드
    if (!EnvManager::Instance().Initialize(env_type)) {
        LOG_ERRORF("CoordinatorServer", "Failed to load environment: %s", env_type.c_str());
        return 1;
    }
    
    try {
        // ========================================
        // 필수 설정 검증
        // ========================================
        std::vector<std::string> required_keys = {
            "COORDINATOR_PLATFORM",
            "NODE_HOSTS",
            "NODE_IDS",
            "NODE_PLATFORMS",
            "NODE_SHARD_INDICES",
            "MPC_THRESHOLD",
            "MPC_TOTAL_SHARDS",
            // HTTPS Server 관련 설정
            "COORDINATOR_HTTPS_PORT",
            "TLS_CERT_COORDINATOR_WALLET",
            "TLS_KMS_COORDINATOR_WALLET_KEY_ID"
        };

        LOG_INFO("CoordinatorServer", "Validating required configuration...");
        Config::ValidateRequired(required_keys);
        LOG_INFO("CoordinatorServer", "✓ All required configurations present");

    } catch (const std::exception& e) {
        LOG_ERRORF("CoordinatorServer", "✗ Configuration error: %s", e.what());
        LOG_ERRORF("CoordinatorServer", "Please check your env/.env.%s file.", env_type.c_str());
        return 1;
    }
    
    try {
        std::string platform_type = Config::GetString("COORDINATOR_PLATFORM");
        PlatformType platform = PlatformTypeFromString(platform_type);
        LOG_INFOF("CoordinatorServer", "=== Initialization ===");
        LOG_INFOF("CoordinatorServer", "Coordinator Platform: %s", platform_type.c_str());

        if (platform == PlatformType::UNKNOWN) {
            LOG_ERRORF("CoordinatorServer", "✗ Unsupported platform type: %s", platform_type.c_str());
            return 1;
        }

        // ========================================
        // 1. 리소스 로더 초기화
        // ========================================
        LOG_INFO("CoordinatorServer", "=== Resource Loader Initialization ===");
        ReadOnlyResLoaderManager::Instance().Initialize(platform);
        LOG_INFO("CoordinatorServer", "✓ Resource loader initialized");

        // ========================================
        // 2. KMS 초기화
        // ========================================
        LOG_INFO("CoordinatorServer", "=== KMS Initialization ===");
        std::string kms_config_path;
        if (platform == PlatformType::LOCAL) {
            kms_config_path = Config::GetString("COORDINATOR_LOCAL_KMS_PATH");
        }
        KMSManager::InitializeLocal(platform, kms_config_path);
        LOG_INFO("CoordinatorServer", "✓ KMS initialized successfully");
        
        // ========================================
        // 3. Coordinator Server 초기화
        // ========================================
        LOG_INFO("CoordinatorServer", "=== Coordinator Server Initialization ===");
        
        CoordinatorServer& coordinator = CoordinatorServer::Instance();
        g_coordinator = &coordinator;
        
        // 시그널 핸들러 등록
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);
        
        if (!coordinator.Initialize()) {
            LOG_ERROR("CoordinatorServer", "Failed to initialize coordinator");
            return 1;
        }
        
        if (!coordinator.Start()) {
            LOG_ERROR("CoordinatorServer", "Failed to start coordinator");
            return 1;
        }
        LOG_INFO("CoordinatorServer", "✓ Coordinator server started");
        
        // ========================================
        // 4. HTTPS Server 초기화 및 시작
        // ========================================
        LOG_INFO("CoordinatorServer", "=== HTTPS Server Initialization ===");

        if (!coordinator.InitializeHttpsServer()) {
            LOG_ERROR("CoordinatorServer", "✗ Failed to initialize HTTPS server");
            return 1;
        }
        
        if (!coordinator.StartHttpsServer()) {
            LOG_ERROR("CoordinatorServer", "✗ Failed to start HTTPS server");
            return 1;
        }
        
        LOG_INFO("CoordinatorServer", "✓ HTTPS server started");
        
        // ========================================
        // 5. Node 설정 로드 및 등록
        // ========================================
        LOG_INFO("CoordinatorServer", "=== Node Configuration ===");
        
        std::vector<std::pair<std::string, uint16_t>> node_endpoints = Config::GetNodeEndpoints("NODE_HOSTS");
        std::vector<std::string> node_ids = Config::GetStringArray("NODE_IDS");
        std::vector<std::string> platforms = Config::GetStringArray("NODE_PLATFORMS");
        std::vector<uint16_t> shard_indices = Config::GetUInt16Array("NODE_SHARD_INDICES");
        uint32_t threshold = Config::GetUInt32("MPC_THRESHOLD");
        uint32_t total_shards = Config::GetUInt32("MPC_TOTAL_SHARDS");
        
        if (node_endpoints.empty()) {
            LOG_ERROR("CoordinatorServer", "No node endpoints configured");
            return 1;
        }

        LOG_INFOF("CoordinatorServer", "  Environment: %s", env_type.c_str());
        LOG_INFOF("CoordinatorServer", "  Platform: %s", platform_type.c_str());
        LOG_INFOF("CoordinatorServer", "  MPC Threshold: %d/%d", threshold, total_shards);
        LOG_INFO("CoordinatorServer", "  Target Nodes:");

        for (size_t i = 0; i < node_endpoints.size(); ++i) {
            const auto& endpoint = node_endpoints[i];
            std::string node_id = (i < node_ids.size()) ? node_ids[i] : "node_" + std::to_string(i + 1);
            std::string node_platform = (i < platforms.size()) ? platforms[i] : "LOCAL";
            uint32_t shard_index = (i < shard_indices.size()) ? shard_indices[i] : i;
            
            LOG_INFOF("CoordinatorServer", "    - %s (%s) at %s:%d [shard %d]", 
                node_id.c_str(), node_platform.c_str(), 
                endpoint.first.c_str(), endpoint.second, shard_index);

            PlatformType node_platform_type = PlatformTypeFromString(node_platform);

            if (!coordinator.RegisterNode(node_id, node_platform_type, endpoint.first, endpoint.second, shard_index)) {
                LOG_ERRORF("CoordinatorServer", "Failed to register node: %s", node_id.c_str());
                return 1;
            }
        }
        LOG_INFO("CoordinatorServer", "✓ All nodes registered");
        LOG_INFOF("CoordinatorServer", "Total nodes registered: %d", node_endpoints.size());

        // ========================================
        // 6. Node 연결
        // ========================================
        LOG_INFO("CoordinatorServer", "=== Node Connection ===");
        LOG_INFO("CoordinatorServer", "Attempting to connect to registered nodes...");

        for (const auto& node_id : node_ids) {
            if (coordinator.ConnectToNode(node_id)) {
                LOG_INFOF("CoordinatorServer", "  ✓ Connected to %s", node_id.c_str());
            } else {
                LOG_ERRORF("CoordinatorServer", "  ✗ Failed to connect to %s", node_id.c_str());
            }
        }
        
        size_t connected_count = coordinator.GetConnectedNodeCount();
        LOG_INFOF("CoordinatorServer", "Connected nodes: %d/%d", connected_count, node_ids.size());

        if (connected_count == 0) {
            LOG_WARN("CoordinatorServer", "No nodes connected");
            LOG_WARN("CoordinatorServer", "  Coordinator is running, but cannot process MPC operations");
            LOG_WARN("CoordinatorServer", "  Start Node servers and they will auto-connect");
        }
        
        // ========================================
        // 7. 시작 정보 출력
        // ========================================
        std::string https_bind = Config::GetString("COORDINATOR_HTTPS_BIND");
        uint16_t https_port = Config::GetUInt16("COORDINATOR_HTTPS_PORT");
        
        LOG_INFO("CoordinatorServer", "========================================");
        LOG_INFO("CoordinatorServer", "  Coordinator Server Running");
        LOG_INFO("CoordinatorServer", "========================================");
        LOG_INFOF("CoordinatorServer", "  Environment: %s", env_type.c_str());
        LOG_INFOF("CoordinatorServer", "  Platform: %s", platform_type.c_str());
        LOG_INFOF("CoordinatorServer", "  MPC Threshold: %d/%d", threshold, total_shards);
        LOG_INFOF("CoordinatorServer", "  Registered Nodes: %d", node_ids.size());
        LOG_INFOF("CoordinatorServer", "  Connected Nodes: %d", connected_count);
        LOG_INFOF("CoordinatorServer", "  HTTPS Server: %s", (coordinator.IsHttpsServerRunning() ? "✓ Running" : "✗ Stopped"));
        LOG_INFOF("CoordinatorServer", "  HTTPS Endpoint: %s:%d", https_bind.c_str(), https_port);
        LOG_INFO("CoordinatorServer", "========================================");
        LOG_INFO("CoordinatorServer", "\nPress Ctrl+C to shutdown gracefully...");
        
        // ========================================
        // 8. 메인 루프
        // ========================================
        {
            std::unique_lock<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.wait(lock, [] { return g_shutdown_requested.load(); });
        }
        
        LOG_INFO("CoordinatorServer", "\nShutdown initiated...");
        coordinator.Stop();

        LOG_INFO("CoordinatorServer", "Coordinator server stopped cleanly");
        return 0;
        
    } catch (const ConfigMissingException& e) {
        LOG_ERRORF("CoordinatorServer", "✗ Configuration Error: %s", e.what());
        LOG_ERRORF("CoordinatorServer", "Please check your env/.env.%s file.", env_type.c_str());
        return 1;
    } catch (const KMSException& e) {
        LOG_ERRORF("CoordinatorServer", "✗ KMS Error: %s", e.what());
        return 1;
    } catch (const std::exception& e) {
        LOG_ERRORF("CoordinatorServer", "✗ Fatal Error: %s", e.what());
        return 1;
    }
}