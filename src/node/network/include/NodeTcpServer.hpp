// src/node/network/include/NodeTcpServer.hpp
#pragma once
#include "common/types/BasicTypes.hpp"
#include "common/utils/threading/ThreadPool.hpp"
#include "common/utils/queue/ThreadSafeQueue.hpp"
#include "common/network/tls/include/TlsConnection.hpp"
#include "common/network/tls/include/TlsContext.hpp"
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
    using namespace protocol::coordinator_node;
    using namespace mpc_engine::network::tls;
    
    using MessageHandler = std::function<NetworkMessage(const NetworkMessage&)>;
    using ConnectionHandler = std::function<void(const NodeConnectionInfo&)>;
    using DisconnectionHandler = std::function<void(const NodeConnectionInfo::DisconnectionInfo&)>;

    struct SecurityConfig {
        std::string trusted_coordinator_ip;
        bool strict_mode = true;
        
        bool IsAllowed(const std::string& ip) const {
            return !strict_mode || (ip == trusted_coordinator_ip);
        }
    };

    struct HandlerContext {
        NetworkMessage request;
        MessageHandler handler;
        utils::ThreadSafeQueue<NetworkMessage>* send_queue;

        HandlerContext(const NetworkMessage& req, MessageHandler h, utils::ThreadSafeQueue<NetworkMessage>* sq)
            : request(req), handler(h), send_queue(sq) {}
    };

    class NodeTcpServer 
    {
    private:
        socket_t server_socket = INVALID_SOCKET_VALUE;
        std::string bind_address;
        uint16_t bind_port;
        
        std::atomic<bool> is_running{false};
        std::atomic<bool> is_initialized{false};
        std::atomic<bool> accepting_connections{true};
        
        // TLS Context
        std::unique_ptr<TlsContext> tls_context;
        
        // Single connection
        std::unique_ptr<NodeConnectionInfo> coordinator_connection;
        mutable std::mutex connection_mutex;
        
        // Threads
        std::thread connection_thread;
        std::thread receive_thread;
        std::thread send_thread;
        
        // ThreadPool
        std::unique_ptr<utils::ThreadPool<HandlerContext>> handler_pool;
        size_t num_handler_threads;
        
        // Send queue
        std::unique_ptr<utils::ThreadSafeQueue<NetworkMessage>> send_queue;
        
        SecurityConfig security_config;
        
        MessageHandler message_handler;
        ConnectionHandler connected_handler;
        DisconnectionHandler disconnected_handler;

        // Statistics
        std::atomic<uint64_t> total_messages_received{0};
        std::atomic<uint64_t> total_messages_sent{0};
        std::atomic<uint64_t> total_messages_processed{0};
        std::atomic<uint64_t> handler_errors{0};

        bool enable_kernel_firewall = false;

    public:
        NodeTcpServer(const std::string& address, uint16_t port, size_t handler_threads);
        ~NodeTcpServer();

        bool Initialize(const std::string& certificate_path, const std::string& private_key_id);
        bool Start();
        void Stop();
        bool IsRunning() const;

        bool PrepareShutdown(uint32_t timeout_ms = 30000);    
        void StopAcceptingConnections();
        uint32_t GetPendingRequests() const;

        void SetMessageHandler(MessageHandler handler);
        void SetConnectedHandler(ConnectionHandler handler);
        void SetDisconnectedHandler(DisconnectionHandler handler);

        void SetTrustedCoordinator(const std::string& ip);
        bool HasActiveConnection() const;
        
        void EnableKernelFirewall(bool enable = true) { enable_kernel_firewall = enable; }
        bool IsKernelFirewallEnabled() const { return enable_kernel_firewall; }
        
        struct ServerStats {
            uint64_t messages_received;
            uint64_t messages_sent;
            uint64_t messages_processed;
            uint64_t handler_errors;
            size_t pending_send_queue;
            size_t active_handlers;
        };
        ServerStats GetStats() const;

    private:
        bool InitializeTlsContext(const std::string& certificate_path, const std::string& private_key_id);
        
        void ConnectionLoop();
        void ReceiveLoop();
        void SendLoop();
        
        static void ProcessMessage(HandlerContext* context);
        
        void HandleCoordinatorConnection(socket_t client_socket, const std::string& client_ip, uint16_t client_port);
        
        bool IsAuthorized(const std::string& client_ip);
        void ForceCloseExistingConnection();
        void SetSocketOptions(socket_t sock);
        
        bool SendMessage(const NetworkMessage& outMessage);
        bool ReceiveMessage(NetworkMessage& outMessage);
        static NetworkMessage CreateErrorResponse(uint16_t original_message_type, const std::string& error_message);
    };
}