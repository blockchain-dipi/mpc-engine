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

    // 플랫폼 타입 변환
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

    // 환경 설정 로드 (선택적)
    EnvConfig env_config;
    bool env_loaded = env_config.LoadFromEnv(env_type);
    if (env_loaded) {
        std::cout << "Environment configuration loaded" << std::endl;
        
        // NODE_IDS와 NODE_HOSTS를 읽어서 자신의 정보 찾기
        std::vector<std::string> node_ids = env_config.GetStringArray("NODE_IDS");
        std::vector<std::pair<std::string, uint16_t>> node_endpoints = env_config.GetNodeEndpoints("NODE_HOSTS");
        
        // node_id가 명령행에서 지정된 경우, 해당하는 포트 찾기
        if (!node_id.empty() && node_ids.size() == node_endpoints.size()) {
            for (size_t i = 0; i < node_ids.size(); ++i) {
                if (node_ids[i] == node_id) {
                    // 명령행에서 포트 지정 안한 경우에만 env에서 가져오기
                    if (argc <= 2 || std::find(argv, argv + argc, "--port") == argv + argc) {
                        port = node_endpoints[i].second;
                        std::cout << "  Found port for " << node_id << ": " << port << std::endl;
                    }
                    break;
                }
            }
        }
        
        // node_id가 지정되지 않은 경우, 포트로 역추적
        if (node_id.empty()) {
            for (size_t i = 0; i < node_endpoints.size(); ++i) {
                if (node_endpoints[i].second == port && i < node_ids.size()) {
                    node_id = node_ids[i];
                    std::cout << "  Found node_id for port " << port << ": " << node_id << std::endl;
                    break;
                }
            }
        }
        
        if (!node_id.empty()) {
            std::cout << "  Using Node ID: " << node_id << std::endl;
        }
    }

    try {
        NodeServer node_server(platform);
        g_node_server = &node_server;

        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);

        // NodeConfig 설정
        NodeConfig config;
        config.bind_address = env_loaded ? env_config.GetString("NODE_ADDRESS", "127.0.0.1") : "127.0.0.1";
        config.bind_port = port;
        config.platform_type = platform;
        
        // node_id가 지정되지 않았으면 기본값 생성
        if (node_id.empty()) {
            node_id = ToString(platform) + "_node_" + std::to_string(port);
        }
        config.node_id = node_id;

        node_server.SetNodeConfig(config);

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

    } catch (const std::exception& e) {
        std::cerr << "Node server error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Node server shutdown complete." << std::endl;
    return 0;
}