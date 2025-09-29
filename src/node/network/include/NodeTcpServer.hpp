// src/node/network/include/NodeTcpServer.hpp
#pragma once
#include "common/types/BasicTypes.hpp"
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include "NodeConnectionInfo.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace mpc_engine::node::network
{
    using MessageHandler = std::function<protocol::coordinator_node::NetworkMessage(const protocol::coordinator_node::NetworkMessage&)>;
    using ConnectionHandler = std::function<void(const NodeConnectionInfo&)>;

    struct SecurityConfig {
        std::string trusted_coordinator_ip;
        bool strict_mode = true;
        
        bool IsAllowed(const std::string& ip) const {
            return strict_mode && (ip == trusted_coordinator_ip);
        }
    };

    class NodeTcpServer 
    {
    private:
        socket_t server_socket = INVALID_SOCKET_VALUE;
        std::string bind_address;
        uint16_t bind_port;
        
        std::atomic<bool> is_running{false};
        std::atomic<bool> is_initialized{false};
        
        // Single connection
        std::unique_ptr<NodeConnectionInfo> coordinator_connection;
        mutable std::mutex connection_mutex;
        
        std::thread connection_thread;
        
        SecurityConfig security_config;
        
        MessageHandler message_handler;
        ConnectionHandler connected_handler;
        ConnectionHandler disconnected_handler;

    public:
        NodeTcpServer(const std::string& address, uint16_t port);
        ~NodeTcpServer();

        bool Initialize();
        bool Start();
        void Stop();
        bool IsRunning() const;

        void SetMessageHandler(MessageHandler handler);
        void SetConnectedHandler(ConnectionHandler handler);
        void SetDisconnectedHandler(ConnectionHandler handler);

        void SetTrustedCoordinator(const std::string& ip);
        bool HasActiveConnection() const;

    private:
        void ConnectionLoop();
        void HandleCoordinatorConnection(socket_t client_socket, const std::string& client_ip, uint16_t client_port);
        
        bool IsAuthorized(const std::string& client_ip);
        void ForceCloseExistingConnection();
        void SetSocketOptions(socket_t sock);
        
        bool SendMessage(socket_t sock, const protocol::coordinator_node::NetworkMessage& message);
        bool ReceiveMessage(socket_t sock, protocol::coordinator_node::NetworkMessage& message);
    };
}