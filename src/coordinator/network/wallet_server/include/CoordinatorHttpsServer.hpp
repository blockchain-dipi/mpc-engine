// src/coordinator/network/wallet_server/include/CoordinatorHttpsServer.hpp
#pragma once

#include "coordinator/network/wallet_server/include/HttpsSession.hpp"
#include "common/utils/threading/ThreadPool.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace mpc_engine::coordinator::network::wallet_server
{
    namespace asio = boost::asio;
    namespace ssl = boost::asio::ssl;
    using tcp = boost::asio::ip::tcp;

    /**
     * @brief HTTPS 서버 설정
     */
    struct HttpsServerConfig 
    {
        std::string bind_address = "0.0.0.0";
        uint16_t bind_port = 9080;
        size_t max_connections = 1000;
        size_t handler_threads = 16;
        size_t io_threads = 0;  // 0 = hardware_concurrency
        
        int max_requests_per_connection = 1000;
        int keep_alive_timeout_sec = 60;
        
        std::string tls_cert_path;
        std::string tls_key_id;  // KMS Key ID
    };

    /**
     * @brief Coordinator HTTPS 서버 (boost.asio 기반)
     * 
     * Wallet 서버들과 HTTPS 통신을 담당합니다.
     * - boost.beast 기반 비동기 I/O
     * - HTTP Keep-Alive 지원
     * - ThreadPool 기반 비지니스 로직 처리
     */
    class CoordinatorHttpsServer 
    {
    public:
        explicit CoordinatorHttpsServer(const HttpsServerConfig& config);
        ~CoordinatorHttpsServer();

        CoordinatorHttpsServer(const CoordinatorHttpsServer&) = delete;
        CoordinatorHttpsServer& operator=(const CoordinatorHttpsServer&) = delete;

        /**
         * @brief 서버 초기화
         * - WalletMessageRouter 초기화
         * - TLS Context 초기화
         * - ThreadPool 생성
         */
        bool Initialize();

        /**
         * @brief 서버 시작
         * - Accept Loop 시작
         * - io_context 멀티스레드 실행
         */
        bool Start();

        /**
         * @brief 서버 중지
         */
        void Stop();
        
        bool IsRunning() const { return running_.load(); }
        bool IsInitialized() const { return initialized_.load(); }

    private:
        /**
         * @brief 비동기 Accept
         */
        void DoAccept();

        /**
         * @brief TLS Context 초기화
         */
        bool InitializeTlsContext();

        /**
         * @brief ThreadPool 초기화
         */
        bool InitializeThreadPool();

        /**
         * @brief WalletMessageRouter 초기화
         */
        bool InitializeRouter();

    private:
        HttpsServerConfig config_;
        
        // Boost.Asio 컴포넌트
        asio::io_context io_context_;
        std::unique_ptr<ssl::context> ssl_context_;
        std::unique_ptr<tcp::acceptor> acceptor_;
        
        // io_context 실행 스레드들
        std::vector<std::thread> io_threads_;
        
        // 비지니스 로직 처리
        std::unique_ptr<ThreadPool<WalletHandlerContext>> handler_pool_;
        
        // Singleton Router 참조는 Initialize에서 체크만 함
        
        std::atomic<bool> running_{false};
        std::atomic<bool> initialized_{false};
    };

} // namespace mpc_engine::coordinator::network::wallet_server