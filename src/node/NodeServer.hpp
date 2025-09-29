// src/node/NodeServer.hpp
#pragma once
#include "common/types/BasicTypes.hpp"
#include "common/types/NodePlatformType.hpp"
#include "node/network/include/NodeTcpServer.hpp"
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <memory>
#include <string>
#include <atomic>

namespace mpc_engine::node
{
    struct NodeConfig 
    {
        std::string node_id;
        NodePlatformType platform_type = NodePlatformType::LOCAL;
        std::string bind_address = "127.0.0.1";
        uint16_t bind_port = 8081;

        bool IsValid() const;
    };

    struct NodeStats 
    {
        std::string node_id;
        NodePlatformType platform_type;
        ConnectionStatus status;
        uint32_t total_requests = 0;
        uint32_t successful_requests = 0;
        uint32_t active_connections = 0;
        uint64_t uptime_seconds = 0;
    };

    class NodeServer 
    {
    private:
        std::unique_ptr<network::NodeTcpServer> tcp_server;
        NodeConfig node_config;
        std::atomic<bool> is_running{false};
        std::atomic<bool> is_initialized{false};
        uint64_t start_time = 0;

    public:
        NodeServer(NodePlatformType platform = NodePlatformType::LOCAL);
        ~NodeServer();

        bool Initialize();
        bool Start();
        void Stop();
        bool IsRunning() const;

        void SetNodeConfig(const NodeConfig& config);
        std::string GetNodeId() const;
        NodePlatformType GetPlatformType() const;
        NodeStats GetStats() const;

    private:
        void OnCoordinatorConnected(const network::NodeConnectionInfo& connection);
        void OnCoordinatorDisconnected(const network::NodeConnectionInfo& connection);
        protocol::coordinator_node::NetworkMessage ProcessMessage(const protocol::coordinator_node::NetworkMessage& message);
        void SetupCallbacks();
        
        // 누락된 메서드 선언들 추가
        std::unique_ptr<protocol::coordinator_node::BaseRequest> ConvertToBaseRequest(const protocol::coordinator_node::NetworkMessage& message);
        protocol::coordinator_node::NetworkMessage ConvertToNetworkMessage(const protocol::coordinator_node::BaseResponse& response);
        protocol::coordinator_node::NetworkMessage CreateErrorResponse(uint16_t messageType, const std::string& errorMessage);
    };
}