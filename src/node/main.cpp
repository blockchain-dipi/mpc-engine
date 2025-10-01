// src/node/main.cpp
#include "NodeServer.hpp"
#include "common/config/EnvConfig.hpp"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

using namespace mpc_engine::node;
using namespace mpc_engine::config;

NodeServer* g_node_server = nullptr;

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_node_server) {
        g_node_server->Stop();
    }
    exit(0);
}

void PrintUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [PLATFORM] [--env ENVIRONMENT] [--port PORT] [--id NODE_ID]" << std::endl;
    std::cout << std::endl;
    std::cout << "Platform:" << std::endl;
    std::cout << "  local       Local development node (default)" << std::endl;
    std::cout << "  aws         AWS Nitro Enclaves node" << std::endl;
    std::cout << "  ibm         IBM Cloud node" << std::endl;
    std::cout << "  azure       Azure node" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " local --port 8081 --id node_1" << std::endl;
    std::cout << "  " << program_name << " aws --env production" << std::endl;
}

void ValidateNodeConfig(const EnvConfig& config) {
    std::vector<std::string> required_keys = {
        "TRUSTED_COORDINATOR_IP",
        "NODE_IDS",
        "NODE_HOSTS"
    };
    
    std::cout << "Validating required configuration..." << std::endl;
    
    try {
        config.ValidateRequired(required_keys);
        std::cout << "✓ All required configurations present" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Configuration validation failed: " << e.what() << std::endl;
        throw;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== MPC Engine Node Server ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << std::endl;

    // 기본값
    std::string platform_str = "local";
    std::string env_type = "local";
    uint16_t port = 8081;
    std::string node_id = "";

    // 명령행 인자 처리
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--env" && i + 1 < argc) {
            env_type = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--id" && i + 1 < argc) {
            node_id = argv[++i];
        } else if (arg[0] != '-') {
            platform_str = arg;
        }
    }

    NodePlatformType platform = FromString(platform_str);
    if (platform == NodePlatformType::UNKNOWN) {
        std::cerr << "Invalid platform: " << platform_str << std::endl;
        return 1;
    }

    std::cout << "Starting node server..." << std::endl;
    std::cout << "  Platform: " << platform_str << std::endl;
    std::cout << "  Environment: " << env_type << std::endl;
    std::cout << "  Port: " << port << std::endl;
    if (!node_id.empty()) {
        std::cout << "  Node ID: " << node_id << std::endl;
    }
    std::cout << std::endl;

    // 환경 설정 로드
    EnvConfig env_config;
    if (!env_config.LoadFromEnv(env_type)) {
        std::cerr << "Failed to load environment configuration: " << env_type << std::endl;
        return 1;
    }

    // 필수 설정 검증
    try {
        ValidateNodeConfig(env_config);
    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << std::endl;
        std::cerr << "\nPlease check your env/.env." << env_type << " file." << std::endl;
        return 1;
    }

    try {
        NodeServer node_server(platform);
        g_node_server = &node_server;

        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);

        NodeConfig config;
        
        // 모든 설정값은 필수 - 예외 발생 가능
        config.bind_address = env_config.GetString("NODE_ADDRESS");
        config.bind_port = port;
        config.platform_type = platform;
        
        std::vector<std::string> node_ids = env_config.GetStringArray("NODE_IDS");
        std::vector<std::pair<std::string, uint16_t>> node_endpoints = env_config.GetNodeEndpoints("NODE_HOSTS");
        
        // node_id 찾기
        if (node_id.empty()) {
            for (size_t i = 0; i < node_endpoints.size(); ++i) {
                if (node_endpoints[i].second == port && i < node_ids.size()) {
                    node_id = node_ids[i];
                    std::cout << "  Found node_id for port " << port << ": " << node_id << std::endl;
                    break;
                }
            }
        }
        
        if (node_id.empty()) {
            node_id = ToString(platform) + "_node_" + std::to_string(port);
        }
        config.node_id = node_id;

        node_server.SetNodeConfig(config);

        // TCP 서버 방화벽 설정
        bool enable_kernel_firewall = env_config.GetBool("ENABLE_KERNEL_FIREWALL");
        std::string trusted_ip = env_config.GetString("TRUSTED_COORDINATOR_IP");
        
        network::NodeTcpServer* tcp_server = node_server.GetTcpServer();
        tcp_server->SetTrustedCoordinator(trusted_ip);
        tcp_server->EnableKernelFirewall(enable_kernel_firewall);

        // 서버 초기화 및 시작
        if (!node_server.Initialize()) {
            std::cerr << "Failed to initialize node server" << std::endl;
            return 1;
        }

        if (!node_server.Start()) {
            std::cerr << "Failed to start node server" << std::endl;
            return 1;
        }

        std::cout << "Node server is running..." << std::endl;
        std::cout << "  Node ID: " << node_server.GetNodeId() << std::endl;
        std::cout << "  Platform: " << ToString(node_server.GetPlatformType()) << std::endl;
        std::cout << "  Address: " << config.bind_address << ":" << config.bind_port << std::endl;
        std::cout << "  Trusted Coordinator: " << trusted_ip << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;
        std::cout << std::endl;

        // 메인 루프
        while (node_server.IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            NodeStats stats = node_server.GetStats();
            std::cout << "[" << stats.uptime_seconds << "s] Node: " << stats.node_id 
                      << ", Status: " << static_cast<int>(stats.status)
                      << ", Active: " << stats.active_connections 
                      << ", Requests: " << stats.total_requests << std::endl;
        }

    } catch (const ConfigMissingException& e) {
        std::cerr << "✗ Configuration error: " << e.what() << std::endl;
        std::cerr << "\nRequired configuration is missing in env/.env." << env_type << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Node server error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Node server shutdown complete." << std::endl;
    return 0;
}