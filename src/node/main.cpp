// src/node/main.cpp
#include "NodeServer.hpp"
#include "types/BasicTypes.hpp"
#include "common/config/EnvManager.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/kms/include/KMSException.hpp"
#include "common/resource/include/ReadOnlyResLoaderManager.hpp"
#include <iostream>
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
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    
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
    std::cout << "Usage: " << program_name << " [--env ENVIRONMENT] --id NODE_ID" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --env ENV      Environment (local, dev, qa, production). Default: local" << std::endl;
    std::cout << "  --id NODE_ID   Node identifier (must match NODE_IDS in config)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " --id node_1" << std::endl;
    std::cout << "  " << program_name << " --env production --id node_aws_1" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== MPC Engine Node Server ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << std::endl;

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
        std::cerr << "Error: --id is required" << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Environment: " << env_type << std::endl;
    std::cout << "  Node ID: " << node_id << std::endl;
    std::cout << std::endl;

    // 환경 설정 로드
    if (!EnvManager::Instance().Initialize(env_type)) {
        std::cerr << "Failed to load environment: " << env_type << std::endl;
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
            std::cerr << "Error: Node ID '" << node_id << "' not found in NODE_IDS" << std::endl;
            std::cerr << "\nAvailable Node IDs:" << std::endl;
            for (const auto& id : node_ids) {
                std::cerr << "  - " << id << std::endl;
            }
            return 1;
        }

        std::cout << "Node configuration:" << std::endl;
        std::cout << "  Node ID: " << node_id << std::endl;
        std::cout << "  Platform: " << PlatformTypeToString(config.platform_type) << std::endl;
        std::cout << "  Bind Address: " << config.bind_address << ":" << config.bind_port << std::endl;
        std::cout << std::endl;

        // readonly resource loader 초기화
        ReadOnlyResLoaderManager::Instance().Initialize(config.platform_type);
        
        // KMS 초기화
        std::cout << "=== KMS Initialization ===" << std::endl;

        std::string kms_config_path;
        if (config.platform_type == PlatformType::LOCAL) {
            kms_config_path = Config::GetString("NODE_LOCAL_KMS_PATH");
        }
        KMSManager::InitializeLocal(PlatformType::UNKNOWN, kms_config_path); // 기본값 설정
        std::cout << "✓ KMS initialized successfully" << std::endl;

        // NodeServer 생성
        NodeServer node_server(config);
        g_node_server = &node_server;

        // 시그널 핸들러
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);

        // 서버 초기화
        if (!node_server.Initialize()) {
            std::cerr << "Failed to initialize node server" << std::endl;
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
            std::cerr << "Failed to start node server" << std::endl;
            return 1;
        }

        // 시작 정보 출력
        std::cout << "========================================" << std::endl;
        std::cout << "  Node Server Running" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Node ID: " << node_server.GetNodeId() << std::endl;
        std::cout << "  Platform: " << PlatformTypeToString(node_server.GetPlatformType()) << std::endl;
        std::cout << "  Listening: " << config.bind_address << ":" << config.bind_port << std::endl;
        std::cout << "  Trusted Coordinator: " << Config::GetString("TRUSTED_COORDINATOR_IP") << std::endl;
        if (tcp_server) {
            std::cout << "  Kernel Firewall: " << (tcp_server->IsKernelFirewallEnabled() ? "ENABLED" : "DISABLED") << std::endl;
        }
        std::cout << "========================================" << std::endl;
        std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
        std::cout << std::endl;

        // 메인 루프
        {
            std::unique_lock<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.wait(lock, []{ 
                return g_shutdown_requested.load() || !g_node_server->IsRunning(); 
            });
        }

    } catch (const ConfigMissingException& e) {
        std::cerr << "Configuration error: " << e.what() << std::endl;
        std::cerr << "\nPlease check your env/.env." << env_type << " file." << std::endl;
        return 1;
    } catch (const KMSException& e) {
        std::cerr << "KMS error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Node server error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Node server shutdown complete." << std::endl;
    return 0;
}