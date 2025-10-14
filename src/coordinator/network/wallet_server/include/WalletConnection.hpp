// src/coordinator/network/wallet_server/include/WalletConnection.hpp
#pragma once

#include "coordinator/network/wallet_server/include/HttpParser.hpp"
#include "common/utils/threading/ThreadPool.hpp"
#include "common/utils/queue/ThreadSafeQueue.hpp"
#include <thread>
#include <atomic>
#include <memory>

namespace mpc_engine::coordinator::network::wallet_server
{
    using namespace mpc_engine::utils;

    struct WalletHandlerContext 
    {
        std::unique_ptr<HttpRequest> request;
        ThreadSafeQueue<HttpResponse>* send_queue;
        
        WalletHandlerContext(std::unique_ptr<HttpRequest> req, ThreadSafeQueue<HttpResponse>* sq)
            : request(std::move(req)), send_queue(sq) {}
    };

    class WalletConnection 
    {
    public:
        WalletConnection(TlsConnection&& tls_conn, ThreadPool<WalletHandlerContext>* handler_pool);
        ~WalletConnection();

        WalletConnection(const WalletConnection&) = delete;
        WalletConnection& operator=(const WalletConnection&) = delete;
        WalletConnection(WalletConnection&&) = delete;
        WalletConnection& operator=(WalletConnection&&) = delete;

        void Start();
        void Stop();
        bool IsActive() const { return active.load(); }
        uint64_t GetIdleTime() const;

    private:
        void ReceiveLoop();
        void SendLoop();
        
        static void ProcessRequest(WalletHandlerContext* context);

        TlsConnection tls_connection;
        ThreadPool<WalletHandlerContext>* handler_pool;
        ThreadSafeQueue<HttpResponse> send_queue;
        
        std::thread receive_thread;
        std::thread send_thread;
        
        std::atomic<bool> active{false};
        std::atomic<uint64_t> last_activity_time{0};
    };

} // namespace mpc_engine::coordinator::network::wallet_server