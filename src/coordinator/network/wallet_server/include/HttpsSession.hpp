// src/coordinator/network/wallet_server/include/HttpsSession.hpp
#pragma once

#include "proto/wallet_coordinator/generated/wallet_message.pb.h"
#include "common/utils/threading/ThreadPool.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <queue>
#include <future>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

// Forward declaration - 올바른 방식
namespace mpc_engine::coordinator::handlers::wallet {
    class WalletMessageRouter;
}

namespace mpc_engine::coordinator::network::wallet_server
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace ssl = boost::asio::ssl;
    using tcp = boost::asio::ip::tcp;

    using namespace mpc_engine::utils;
    using namespace mpc_engine::proto::wallet_coordinator;

    /**
     * @brief HTTP Response 구조체
     */
    struct HttpResponse 
    {
        int status_code = 200;
        std::string status_text = "OK";
        std::unique_ptr<WalletCoordinatorMessage> protobuf_message;

        HttpResponse() = default;
        HttpResponse(HttpResponse&&) = default;
        HttpResponse& operator=(HttpResponse&&) = default;

        HttpResponse(const HttpResponse&) = delete;
        HttpResponse& operator=(const HttpResponse&) = delete;
    };

    /**
     * @brief WalletHandlerContext - ThreadPool용 컨텍스트
     */
    struct WalletHandlerContext 
    {
        std::unique_ptr<WalletCoordinatorMessage> request;
        std::shared_ptr<std::promise<HttpResponse>> promise;

        WalletHandlerContext(
            std::unique_ptr<WalletCoordinatorMessage> req,
            std::shared_ptr<std::promise<HttpResponse>> p)
            : request(std::move(req)), promise(p) {}
    };

    /**
     * @brief HTTPS 세션 (boost.beast 기반)
     * 
     * 하나의 HTTPS 연결을 관리합니다.
     * - 비동기 Read/Write
     * - HTTP 순서 보장 (pending_queue)
     * - Keep-Alive 지원
     */
    class HttpsSession : public std::enable_shared_from_this<HttpsSession> 
    {
    public:
        HttpsSession(
            tcp::socket socket,
            ssl::context& ssl_context,
            ThreadPool<WalletHandlerContext>& thread_pool,
            mpc_engine::coordinator::handlers::wallet::WalletMessageRouter& router,
            int max_requests,
            std::chrono::seconds timeout
        );

        ~HttpsSession();

        /**
         * @brief 세션 시작 (TLS 핸드셰이크 → Read/Write)
         */
        void Start();

        /**
         * @brief 세션 중지
         */
        void Stop();

    private:
        /**
         * @brief TLS 핸드셰이크
         */
        void DoTlsHandshake();

        /**
         * @brief 비동기 Read (여러 요청 동시 수신)
         */
        void DoRead();

        /**
         * @brief Read 완료 콜백
         */
        void OnRead(beast::error_code ec, std::size_t bytes_transferred);

        /**
         * @brief 비동기 Write (순서 보장)
         */
        void DoWrite();

        /**
         * @brief Write 완료 콜백
         */
        void OnWrite(beast::error_code ec, std::size_t bytes_transferred, bool keep_alive);

        /**
         * @brief 연결 종료
         */
        void DoClose();

        /**
         * @brief Keep-Alive 판단
         */
        bool ShouldKeepAlive() const;

        /**
         * @brief 마지막 활동 시간 업데이트
         */
        void UpdateActivity();

        /**
         * @brief 요청 처리 (ThreadPool에서 실행)
         */
        static void ProcessRequest(WalletHandlerContext* context);

    private:
        // Boost.Beast 컴포넌트
        beast::ssl_stream<beast::tcp_stream> stream_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> request_;

        // 순서 보장용 Queue
        std::queue<std::future<HttpResponse>> pending_queue_;
        std::mutex queue_mutex_;
        std::atomic<bool> write_in_progress_{false};

        // 외부 의존성 (참조)
        ThreadPool<WalletHandlerContext>& thread_pool_;
        mpc_engine::coordinator::handlers::wallet::WalletMessageRouter& router_;

        // Keep-Alive 관리
        int requests_handled_ = 0;
        int max_requests_;
        std::chrono::steady_clock::time_point last_activity_;
        std::chrono::seconds timeout_;

        std::atomic<bool> active_{false};
    };

} // namespace mpc_engine::coordinator::network::wallet_server