// src/node/main.cpp
#include "NodeServer.hpp"
#include "types/BasicTypes.hpp"
#include "common/env/EnvManager.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/kms/include/KMSException.hpp"
#include "common/resource/include/ReadOnlyResLoaderManager.hpp"
#include "common/utils/logger/Logger.hpp"
#include <signal.h>
#include <atomic>
#include <condition_variable>
#include <mutex>

using namespace mpc_engine;
using namespace mpc_engine::node;
using namespace mpc_engine::env;
using namespace mpc_engine::kms;
using namespace mpc_engine::node::network;
using namespace mpc_engine::resource;

// 전역 상태 관리 (시그널 핸들러용)
static NodeServer* g_node_server = nullptr;
static std::atomic<bool> g_shutdown_requested{false};
static std::condition_variable g_shutdown_cv;
static std::mutex g_shutdown_mutex;

void SignalHandler(int signal) {
    LOG_INFOF("NodeTcpServer", "Received signal %d, shutting down gracefully...", signal);
    
    if (g_node_server) {
        g_node_server->Stop();
    }
    
    {
        std::lock_guard<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_requested.store(true);
    }
    g_shutdown_cv.notify_one();
}

void PrintUsage(const char* program_name) {
    LOG_INFOF("NodeTcpServer", "Usage: %s [--env ENVIRONMENT] --id NODE_ID", program_name);
    LOG_INFO("NodeTcpServer", "");
    LOG_INFOF("NodeTcpServer", "Usage: %s [--env ENVIRONMENT] --id NODE_ID", program_name);
    LOG_INFO("NodeTcpServer", "");
    LOG_INFO("NodeTcpServer", "Options:");
    LOG_INFO("NodeTcpServer", "  --env ENV      Environment (local, dev, qa, production). Default: local");
    LOG_INFO("NodeTcpServer", "  --id NODE_ID   Node identifier (must match NODE_IDS in config)");
    LOG_INFO("NodeTcpServer", "");
    LOG_INFO("NodeTcpServer", "Examples:");
    LOG_INFOF("NodeTcpServer", "  %s --id node_1", program_name);
    LOG_INFOF("NodeTcpServer", "  %s --env production --id node_aws_1", program_name);
}

int main(int argc, char* argv[]) {
    LOG_INFO("NodeTcpServer", "=== MPC Engine Node Server ===");
    LOG_INFOF("NodeTcpServer", "Build: %s %s", __DATE__, __TIME__);

    // 명령행 인자 파싱
    std::string env_type = "local";
    std::string node_id = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--env" && i + 1 < argc) {
            env_type = argv[++i];
        } else if (arg == "--id" && i + 1 < argc) {
            node_id = argv[++i];
        }
    }

    if (node_id.empty()) {
        LOG_ERROR("NodeTcpServer", "Error: --id is required");
        PrintUsage(argv[0]);
        return 1;
    }

    LOG_INFO("NodeTcpServer", "Configuration:");
    LOG_INFOF("NodeTcpServer", "  Environment: %s", env_type.c_str());
    LOG_INFOF("NodeTcpServer", "  Node ID: %s", node_id.c_str());

    // 환경 설정 로드
    if (!EnvManager::Instance().Initialize(env_type)) {
        LOG_ERRORF("NodeTcpServer", "Failed to load environment: %s", env_type.c_str());
        return 1;
    }

    try {
        // 필수 설정 검증
        std::vector<std::string> required_keys = {
            "TRUSTED_COORDINATOR_IP",
            "NODE_IDS",
            "NODE_HOSTS",
            "NODE_PLATFORMS"
        };
        Config::ValidateRequired(required_keys);

        // Node ID로 설정 찾기
        std::vector<std::string> node_ids = Config::GetStringArray("NODE_IDS");
        std::vector<std::pair<std::string, uint16_t>> node_hosts = Config::GetNodeEndpoints("NODE_HOSTS");
        std::vector<std::string> platforms = Config::GetStringArray("NODE_PLATFORMS");
        std::vector<std::string> tls_cert_paths = Config::GetStringArray("TLS_CERT_PATHS");
        std::vector<std::string> tls_kms_nodes_coordinator_key_ids = Config::GetStringArray("TLS_KMS_NODES_COORDINATOR_KEY_IDS");

        // NodeConfig 설정
        NodeConfig config;
        config.node_id = node_id;

        bool found = false;
        for (size_t i = 0; i < node_ids.size(); ++i) {
            if (node_ids[i] == node_id) {
                if (i < node_hosts.size() && i < platforms.size()) {
                    config.bind_address = node_hosts[i].first;
                    config.bind_port = node_hosts[i].second;
                    config.platform_type = PlatformTypeFromString(platforms[i]);
                    config.certificate_path = tls_cert_paths[i];
                    config.private_key_id = tls_kms_nodes_coordinator_key_ids[i];
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            LOG_ERRORF("NodeTcpServer", "Error: Node ID '%s' not found in NODE_IDS", node_id.c_str());
            LOG_INFO("NodeTcpServer", "Available Node IDs:");
            for (const auto& id : node_ids) {
                LOG_INFOF("NodeTcpServer", "  - %s", id.c_str());
            }
            return 1;
        }

        LOG_INFO("NodeTcpServer", "Node configuration:");
        LOG_INFOF("NodeTcpServer", "  Node ID: %s", node_id.c_str());
        LOG_INFOF("NodeTcpServer", "  Platform: %s", PlatformTypeToString(config.platform_type).c_str());
        LOG_INFOF("NodeTcpServer", "  Bind Address: %s:%d", config.bind_address.c_str(), config.bind_port);
        LOG_INFO("NodeTcpServer", "");

        // readonly resource loader 초기화
        ReadOnlyResLoaderManager::Instance().Initialize(config.platform_type);
        
        // KMS 초기화
        LOG_INFO("NodeTcpServer", "=== KMS Initialization ===");

        std::string kms_config_path;
        if (config.platform_type == PlatformType::LOCAL) {
            kms_config_path = Config::GetString("NODE_LOCAL_KMS_PATH");
        }
        KMSManager::InitializeLocal(config.platform_type, kms_config_path); // 기본값 설정
        LOG_INFO("NodeTcpServer", "✓ KMS initialized successfully");

        // NodeServer 생성
        NodeServer node_server(config);
        g_node_server = &node_server;

        // 시그널 핸들러
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);

        // 서버 초기화
        if (!node_server.Initialize()) {
            LOG_ERROR("NodeTcpServer", "Failed to initialize node server");
            return 1;
        }

        // 보안 설정
        NodeTcpServer* tcp_server = node_server.GetTcpServer();
        if (tcp_server) {
            std::string trusted_ip = Config::GetString("TRUSTED_COORDINATOR_IP");
            tcp_server->SetTrustedCoordinator(trusted_ip);

            bool enable_firewall = Config::HasKey("ENABLE_KERNEL_FIREWALL") ? Config::GetBool("ENABLE_KERNEL_FIREWALL") : false;
            tcp_server->EnableKernelFirewall(enable_firewall);
        }

        // 서버 시작
        if (!node_server.Start()) {
            LOG_ERROR("NodeTcpServer", "Failed to start node server");
            return 1;
        }

        // 시작 정보 출력
        LOG_INFO("NodeTcpServer", "========================================");
        LOG_INFO("NodeTcpServer", "  Node Server Running");
        LOG_INFO("NodeTcpServer", "========================================");
        LOG_INFOF("NodeTcpServer", "  Node ID: %s", node_server.GetNodeId().c_str());
        LOG_INFOF("NodeTcpServer", "  Platform: %s", PlatformTypeToString(node_server.GetPlatformType()).c_str());
        LOG_INFOF("NodeTcpServer", "  Listening: %s:%d", config.bind_address.c_str(), config.bind_port);
        LOG_INFOF("NodeTcpServer", "  Trusted Coordinator: %s", Config::GetString("TRUSTED_COORDINATOR_IP").c_str());
        if (tcp_server) {
            LOG_INFOF("NodeTcpServer", "  Kernel Firewall: %s", (tcp_server->IsKernelFirewallEnabled() ? "ENABLED" : "DISABLED"));
        }
        LOG_INFO("NodeTcpServer", "========================================");
        LOG_INFO("NodeTcpServer", "Server is running. Press Ctrl+C to stop.");
        LOG_INFO("NodeTcpServer", "");

        // 메인 루프
        {
            std::unique_lock<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.wait(lock, []{ 
                return g_shutdown_requested.load() || !g_node_server->IsRunning(); 
            });
        }

    } catch (const ConfigMissingException& e) {
        LOG_ERRORF("NodeTcpServer", "Configuration error: %s", e.what());
        LOG_ERRORF("NodeTcpServer", "Please check your env/.env.%s file.", env_type.c_str());
        return 1;
    } catch (const KMSException& e) {
        LOG_ERRORF("NodeTcpServer", "KMS error: %s", e.what());
        return 1;
    } catch (const std::exception& e) {
        LOG_ERRORF("NodeTcpServer", "Node server error: %s", e.what());
        return 1;
    }

    LOG_INFO("NodeTcpServer", "Node server shutdown complete.");
    return 0;
}