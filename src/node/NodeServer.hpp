// src/node/NodeServer.hpp
#pragma once
#include "common/types/BasicTypes.hpp"
#include "node/network/include/NodeTcpServer.hpp"
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <memory>
#include <string>
#include <atomic>

namespace mpc_engine::node
{    
    using namespace protocol::coordinator_node;

    struct NodeConfig 
    {
        std::string node_id;
        PlatformType platform_type = PlatformType::LOCAL;
        std::string bind_address = "127.0.0.1";
        uint16_t bind_port = 8081;
        std::string certificate_path;
        std::string private_key_id;

        bool IsValid() const;
    };

    struct NodeStats 
    {
        std::string node_id;
        PlatformType platform_type;
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
        NodeServer(const NodeConfig& config);
        ~NodeServer();

        bool Initialize();
        bool Start();
        void Stop();
        bool IsRunning() const;

        // TCP 서버 접근 (방화벽 설정용)
        network::NodeTcpServer* GetTcpServer() { return tcp_server.get(); }
        
        std::string GetNodeId() const;
        PlatformType GetPlatformType() const;
        NodeStats GetStats() const;

    private:
        void OnCoordinatorConnected(const network::NodeConnectionInfo& connection);
        void OnCoordinatorDisconnected(const network::NodeConnectionInfo::DisconnectionInfo& connection);
        NetworkMessage ProcessMessage(const NetworkMessage& message);
        void SetupCallbacks();
        
        // 누락된 메서드 선언들 추가
        std::unique_ptr<BaseRequest> ConvertToBaseRequest(const NetworkMessage& message);
        NetworkMessage ConvertToNetworkMessage(const BaseResponse& response);
        NetworkMessage CreateErrorResponse(uint16_t messageType, const std::string& errorMessage);
    };
}