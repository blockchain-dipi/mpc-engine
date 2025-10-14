// src/coordinator/network/wallet_server/include/CoordinatorHttpsServer.hpp
#pragma once

#include "coordinator/network/wallet_server/include/WalletConnection.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include "common/network/tls/include/TlsConnection.hpp"
#include "common/utils/threading/ThreadPool.hpp"
#include "types/BasicTypes.hpp"
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace mpc_engine::coordinator::network::wallet_server
{
    struct HttpsServerConfig 
    {
        std::string bind_address = "0.0.0.0";
        uint16_t bind_port = 8080;
        size_t max_connections = 100;
        size_t handler_threads = 8;
        uint32_t idle_timeout_ms = 60000;  // 60초
        
        std::string tls_cert_path;
        std::string tls_key_id;  // KMS Key ID
    };

    class CoordinatorHttpsServer 
    {
    public:
        explicit CoordinatorHttpsServer(const HttpsServerConfig& config);
        ~CoordinatorHttpsServer();

        CoordinatorHttpsServer(const CoordinatorHttpsServer&) = delete;
        CoordinatorHttpsServer& operator=(const CoordinatorHttpsServer&) = delete;

        bool Initialize();
        bool Start();
        void Stop();
        
        bool IsRunning() const { return running.load(); }
        size_t GetActiveConnectionCount() const;

    private:
        void AcceptLoop();
        void CleanupLoop();
        void RemoveInactiveConnections();

        bool InitializeTlsContext();
        bool CreateListenSocket();

        HttpsServerConfig config;
        
        socket_t listen_socket = INVALID_SOCKET_VALUE;
        std::unique_ptr<TlsContext> tls_context;
        std::unique_ptr<ThreadPool<WalletHandlerContext>> handler_pool;
        
        std::vector<std::unique_ptr<WalletConnection>> connections;
        mutable std::mutex connections_mutex;
        
        std::thread accept_thread;
        std::thread cleanup_thread;
        
        std::atomic<bool> running{false};
        std::atomic<bool> initialized{false};
    };

} // namespace mpc_engine::coordinator::network::wallet_server