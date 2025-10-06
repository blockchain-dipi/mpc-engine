// src/node/network/include/NodeTcpServer.hpp
#pragma once
#include "common/types/BasicTypes.hpp"
#include "common/utils/threading/ThreadPool.hpp"
#include "common/utils/queue/ThreadSafeQueue.hpp"
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
    using MessageHandler = std::function<NetworkMessage(const NetworkMessage&)>;
    using ConnectionHandler = std::function<void(const NodeConnectionInfo&)>;

    struct SecurityConfig {
        std::string trusted_coordinator_ip;
        bool strict_mode = true;
        
        bool IsAllowed(const std::string& ip) const {
            return !strict_mode || (ip == trusted_coordinator_ip);
        }
    };

    // Handler Worker Context
    struct HandlerContext {
        NetworkMessage request;
        MessageHandler handler;
        utils::ThreadSafeQueue<NetworkMessage>* send_queue;

        HandlerContext(const NetworkMessage& req,
            MessageHandler h,
            utils::ThreadSafeQueue<NetworkMessage>* sq)
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
        
        // Single connection info
        std::unique_ptr<NodeConnectionInfo> coordinator_connection;
        mutable std::mutex connection_mutex;
        
        // Thread components
        std::thread connection_thread;
        std::thread receive_thread;
        std::thread send_thread;
        
        // ThreadPool for message handlers
        std::unique_ptr<utils::ThreadPool<HandlerContext>> handler_pool;
        size_t num_handler_threads;
        
        // Send queue
        std::unique_ptr<utils::ThreadSafeQueue<NetworkMessage>> send_queue;
        
        SecurityConfig security_config;
        
        MessageHandler message_handler;
        ConnectionHandler connected_handler;
        ConnectionHandler disconnected_handler;

        // Statistics
        std::atomic<uint64_t> total_messages_received{0};
        std::atomic<uint64_t> total_messages_sent{0};
        std::atomic<uint64_t> total_messages_processed{0};
        std::atomic<uint64_t> handler_errors{0};

        // Kernel firewall
        bool enable_kernel_firewall = false;

        // 새 연결 수락 제어
        std::atomic<bool> accepting_connections{true};

    public:
        NodeTcpServer(const std::string& address, uint16_t port, size_t handler_threads);
        ~NodeTcpServer();

        bool Initialize();
        bool Start();
        void Stop();
        bool IsRunning() const;

        bool PrepareShutdown(uint32_t timeout_ms = 30000);    
        void StopAcceptingConnections();
        uint32_t GetPendingRequests() const;

        void SetMessageHandler(MessageHandler handler);
        void SetConnectedHandler(ConnectionHandler handler);
        void SetDisconnectedHandler(ConnectionHandler handler);

        void SetTrustedCoordinator(const std::string& ip);
        bool HasActiveConnection() const;
        
        // 커널 방화벽 설정 (옵션)
        void EnableKernelFirewall(bool enable = true) { enable_kernel_firewall = enable; }
        bool IsKernelFirewallEnabled() const { return enable_kernel_firewall; }
        
        // Statistics
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
        // Thread functions
        void ConnectionLoop();
        void ReceiveLoop();
        void SendLoop();
        
        // Handler worker function (static for ThreadPool)
        static void ProcessMessage(HandlerContext* context);
        
        void HandleCoordinatorConnection(socket_t client_socket, const std::string& client_ip, uint16_t client_port);
        
        bool IsAuthorized(const std::string& client_ip);
        void ForceCloseExistingConnection();
        void SetSocketOptions(socket_t sock);
        
        bool SendMessage(socket_t sock, const NetworkMessage& outMessage);
        bool ReceiveMessage(socket_t sock, NetworkMessage& outMessage);
        static NetworkMessage CreateErrorResponse(uint16_t original_message_type, const std::string& error_message);
    };
}