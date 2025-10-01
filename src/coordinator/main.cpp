// src/coordinator/main.cpp
#include "CoordinatorServer.hpp"
#include "common/types/BasicTypes.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/config/EnvConfig.hpp"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <vector>

using namespace mpc_engine::coordinator;
using namespace mpc_engine::node;
using namespace mpc_engine::config;

CoordinatorServer* g_coordinator = nullptr;

void SignalHandler(int signal) 
{
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    
    if (g_coordinator) 
    {
        g_coordinator->Stop();
    }
    exit(0);
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

void PrintSystemStatus(CoordinatorServer& coordinator) 
{
    CoordinatorStats stats = coordinator.GetStats();
    std::cout << "[" << mpc_engine::utils::GetCurrentTimeMs() << "] Coordinator Stats:" << std::endl;
    std::cout << "  Total Nodes: " << stats.total_nodes << std::endl;
    std::cout << "  Connected Nodes: " << stats.connected_nodes << std::endl;
    std::cout << "  Ready Nodes: " << stats.ready_nodes << std::endl;
    std::cout << "  Uptime: " << stats.uptime_seconds << "s" << std::endl;
    
    std::vector<std::string> connected_nodes = coordinator.GetConnectedNodeIds();
    if (!connected_nodes.empty()) {
        std::cout << "  Connected Nodes: ";
        for (const std::string& node_id : connected_nodes) {
            std::cout << node_id << "(" << coordinator.GetNodeEndpoint(node_id) << ") ";
        }
        std::cout << std::endl;
    }
    
    std::vector<std::string> local_nodes = coordinator.GetNodesByPlatform(NodePlatformType::LOCAL);
    std::vector<std::string> aws_nodes = coordinator.GetNodesByPlatform(NodePlatformType::AWS);
    std::vector<std::string> ibm_nodes = coordinator.GetNodesByPlatform(NodePlatformType::IBM);
    std::vector<std::string> azure_nodes = coordinator.GetNodesByPlatform(NodePlatformType::AZURE);
    
    std::cout << "  Platform Distribution: LOCAL=" << local_nodes.size() 
              << ", AWS=" << aws_nodes.size()
              << ", IBM=" << ibm_nodes.size() 
              << ", Azure=" << azure_nodes.size() << std::endl;
    std::cout << std::endl;
}

void ValidateCoordinatorConfig(const EnvConfig& config) {
    std::vector<std::string> required_keys = {
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
        CoordinatorServer& coordinator = CoordinatorServer::Instance();
        g_coordinator = &coordinator;
        
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
        
        std::vector<std::pair<std::string, uint16_t>> node_endpoints = env_config.GetNodeEndpoints("NODE_HOSTS");
        if (node_endpoints.empty()) {
            std::cerr << "No node endpoints configured" << std::endl;
            return 1;
        }
        
        std::cout << "Coordinator configuration:" << std::endl;
        std::cout << "  Environment: " << env_type << std::endl;
        std::cout << "  Target Nodes:" << std::endl;
        for (const auto& endpoint : node_endpoints) {
            std::cout << "    " << endpoint.first << ":" << endpoint.second << std::endl;
        }
        std::cout << std::endl;
        
        std::cout << "Registering nodes..." << std::endl;
        std::vector<std::pair<std::string, uint16_t>> node_endpoints = env_config.GetNodeEndpoints("NODE_HOSTS");
        std::vector<std::string> node_ids = env_config.GetStringArray("NODE_IDS");
        std::vector<std::string> platforms = env_config.GetStringArray("NODE_PLATFORMS");
        std::vector<uint16_t> shard_indices = env_config.GetUInt16Array("NODE_SHARD_INDICES");
        uint32_t threshold = env_config.GetUInt32("MPC_THRESHOLD");
        uint32_t total_shards = env_config.GetUInt32("MPC_TOTAL_SHARDS");
            
        for (size_t i = 0; i < node_endpoints.size(); ++i) {
            const auto& endpoint = node_endpoints[i];
            
            std::string node_id = (i < node_ids.size()) ? node_ids[i] : "node_" + std::to_string(i + 1);
            NodePlatformType platform = (i < platforms.size()) ? FromString(platforms[i]) : NodePlatformType::LOCAL;
            uint32_t shard_index = (i < shard_indices.size()) ? shard_indices[i] : static_cast<uint32_t>(i);
            
            if (coordinator.RegisterNode(node_id, platform, endpoint.first, endpoint.second, shard_index)) {
                std::cout << "  Registered: " << node_id << " at " << endpoint.first << ":" << endpoint.second << std::endl;
            }
        }
        
        std::cout << "\nCoordinator is running... Press Ctrl+C to stop." << std::endl;
        std::cout << "MPC Configuration:" << std::endl;
        std::cout << "  Threshold: " << threshold << std::endl;
        std::cout << "  Total Shards: " << total_shards << std::endl;
        std::cout << std::endl;
        
        std::cout << "Connecting to all nodes..." << std::endl;
        for (const auto& node_id : coordinator.GetAllNodeIds()) {
            if (coordinator.ConnectToNode(node_id)) {
                std::cout << "  Connected to: " << node_id << std::endl;
            } else {
                std::cout << "  Failed to connect to: " << node_id << std::endl;
            }
        }
        std::cout << std::endl;

        while (coordinator.IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            PrintSystemStatus(coordinator);
        }
        
    } catch (const ConfigMissingException& e) {
        std::cerr << "✗ Configuration error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Coordinator error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Coordinator shutdown complete." << std::endl;
    return 0;
}