// src/coordinator/network/wallet_server/src/HttpsSession.cpp
#include "coordinator/network/wallet_server/include/HttpsSession.hpp"
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include <iostream>

namespace mpc_engine::coordinator::network::wallet_server
{
    using namespace mpc_engine::coordinator::handlers::wallet;
    using std::chrono::steady_clock;

    HttpsSession::HttpsSession(
        tcp::socket socket,
        ssl::context& ssl_context,
        ThreadPool<WalletHandlerContext>& thread_pool,
        WalletMessageRouter& router,
        int max_requests,
        std::chrono::seconds timeout
    )
        : stream_(std::move(socket), ssl_context)
        , thread_pool_(thread_pool)
        , router_(router)
        , max_requests_(max_requests)
        , timeout_(timeout)
    {
        last_activity_ = steady_clock::now();
    }

    HttpsSession::~HttpsSession() 
    {
        Stop();
    }

    void HttpsSession::Start() 
    {
        if (active_.exchange(true)) {
            return;
        }

        std::cout << "[HttpsSession] Starting..." << std::endl;
        DoTlsHandshake();
    }

    void HttpsSession::Stop() 
    {
        if (!active_.exchange(false)) {
            return;
        }

        std::cout << "[HttpsSession] Stopping..." << std::endl;

        beast::error_code ec;

        // Shutdown socket (양방향)
        auto& socket = beast::get_lowest_layer(stream_).socket();
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        socket.shutdown(tcp::socket::shutdown_both, ec);
        // shutdown 에러는 무시 (이미 닫혔을 수 있음)

        // Close socket
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        socket.close(ec);
        if (ec) {
            std::cerr << "[HttpsSession] Close error: " << ec.message() << std::endl;
        }
    }

    void HttpsSession::DoTlsHandshake() 
    {
        auto self = shared_from_this();

        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        stream_.async_handshake(
            ssl::stream_base::server,
            [self](beast::error_code ec) {
                if (!ec) {
                    std::cout << "[HttpsSession] TLS handshake success" << std::endl;
                    self->DoRead();
                } else {
                    std::cerr << "[HttpsSession] TLS handshake failed: " 
                              << ec.message() << std::endl;
                }
            }
        );
    }

    void HttpsSession::DoRead() 
    {
        if (!active_.load()) {
            return;
        }

        auto self = shared_from_this();

        request_ = {};

        // 타임아웃 설정
        beast::get_lowest_layer(stream_).expires_after(timeout_);

        http::async_read(
            stream_,
            buffer_,
            request_,
            [self](beast::error_code ec, std::size_t bytes) {
                self->OnRead(ec, bytes);
            }
        );
    }

    void HttpsSession::OnRead(beast::error_code ec, std::size_t bytes_transferred) 
    {
        (void)bytes_transferred;

        if (ec == http::error::end_of_stream) {
            std::cout << "[HttpsSession] Client closed connection" << std::endl;
            DoClose();
            return;
        }

        if (ec == beast::error::timeout) {
            std::cout << "[HttpsSession] Idle timeout" << std::endl;
            DoClose();
            return;
        }

        if (ec) {
            std::cerr << "[HttpsSession] Read error: " << ec.message() << std::endl;
            DoClose();
            return;
        }

        UpdateActivity();

        std::cout << "[HttpsSession] Received request: " 
                  << request_.method_string() << " " 
                  << request_.target() << std::endl;

        // Proto 역직렬화
        auto wallet_message = std::make_unique<WalletCoordinatorMessage>();
        if (!wallet_message->ParseFromString(request_.body())) {
            std::cerr << "[HttpsSession] Proto parse failed" << std::endl;
            
            // TODO: 400 Bad Request 응답
            // 지금은 그냥 다음 요청 읽기
            DoRead();
            return;
        }

        // promise/future 생성
        auto promise = std::make_shared<std::promise<HttpResponse>>();
        auto future = promise->get_future();

        // Context 생성
        auto context = std::make_unique<WalletHandlerContext>(
            std::move(wallet_message),
            promise
        );

        // pending_queue에 future 추가 (순서 기록!)
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            pending_queue_.push(std::move(future));
        }

        // ThreadPool에 제출
        try {
            thread_pool_.SubmitOwned(ProcessRequest, std::move(context));
        } catch (const std::exception& e) {
            std::cerr << "[HttpsSession] Failed to submit task: " << e.what() << std::endl;
            DoClose();
            return;
        }

        requests_handled_++;

        // 즉시 다음 요청 읽기 (비동기!)
        DoRead();

        // Write Loop 시작 (첫 요청이면)
        bool expected = false;
        if (write_in_progress_.compare_exchange_strong(expected, true)) {
            DoWrite();
        }
    }

    void HttpsSession::DoWrite() 
    {
        std::future<HttpResponse> future;

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (pending_queue_.empty()) {
                write_in_progress_ = false;
                return;
            }

            future = std::move(pending_queue_.front());
            pending_queue_.pop();
        }

        // 첫 번째 요청 완료 대기 (순서 보장!)
        HttpResponse http_response;
        try {
            http_response = future.get();  // blocking
        } catch (const std::exception& e) {
            std::cerr << "[HttpsSession] Future exception: " << e.what() << std::endl;
            DoClose();
            return;
        }

        // Proto 직렬화
        std::string proto_body;
        if (http_response.protobuf_message && 
            !http_response.protobuf_message->SerializeToString(&proto_body)) {
            std::cerr << "[HttpsSession] Proto serialize failed" << std::endl;
            DoClose();
            return;
        }

        // HTTP Response 생성
        auto response = std::make_shared<http::response<http::string_body>>();
        response->version(11);
        response->result(http_response.status_code);
        response->set(http::field::content_type, "application/x-protobuf");
        response->body() = std::move(proto_body);

        // Keep-Alive 결정
        bool keep_alive = ShouldKeepAlive();
        response->keep_alive(keep_alive);
        response->prepare_payload();

        std::cout << "[HttpsSession] Sending response: " 
                  << http_response.status_code 
                  << " (Keep-Alive: " << (keep_alive ? "yes" : "no") << ")" << std::endl;

        auto self = shared_from_this();

        http::async_write(
            stream_,
            *response,
            [self, response, keep_alive](beast::error_code ec, std::size_t bytes) {
                self->OnWrite(ec, bytes, keep_alive);
            }
        );
    }

    void HttpsSession::OnWrite(beast::error_code ec, std::size_t bytes_transferred, bool keep_alive) 
    {
        (void)bytes_transferred;

        if (ec) {
            std::cerr << "[HttpsSession] Write error: " << ec.message() << std::endl;
            DoClose();
            return;
        }

        UpdateActivity();

        if (!keep_alive) {
            std::cout << "[HttpsSession] Keep-Alive expired, closing connection" << std::endl;
            DoClose();
            return;
        }

        // 다음 응답 처리
        DoWrite();
    }

    void HttpsSession::DoClose() 
    {
        if (!active_.load()) {
            return;
        }
    
        std::cout << "[HttpsSession] Closing connection" << std::endl;
    
        auto self = shared_from_this();
    
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(5));
    
        stream_.async_shutdown(
            [self](beast::error_code ec) {
                // SSL shutdown 에러 처리
                // eof는 정상 종료이므로 무시
                if (ec && ec != boost::asio::error::eof) {
                    // SSL short read는 정상적인 종료일 수 있음
                    if (ec != boost::asio::ssl::error::stream_truncated) {
                        std::cerr << "[HttpsSession] SSL shutdown error: " << ec.message() << std::endl;
                    }
                }
                
                // Socket close
                beast::error_code close_ec;
                auto& socket = beast::get_lowest_layer(self->stream_).socket();
                if (socket.is_open()) {
                    // NOLINTNEXTLINE(bugprone-unused-return-value)
                    socket.close(close_ec);
                    // close 에러는 로그만 (이미 닫혔을 수 있음)
                    if (close_ec) {
                        std::cerr << "[HttpsSession] Socket close error: " << close_ec.message() << std::endl;
                    }
                }
            }
        );
    
        active_ = false;
    }

    bool HttpsSession::ShouldKeepAlive() const 
    {
        if (requests_handled_ >= max_requests_) {
            return false;
        }

        auto elapsed = steady_clock::now() - last_activity_;
        if (elapsed > timeout_) {
            return false;
        }

        return true;
    }

    void HttpsSession::UpdateActivity() 
    {
        last_activity_ = steady_clock::now();
    }

    void HttpsSession::ProcessRequest(WalletHandlerContext* context) 
    {
        if (!context || !context->request || !context->promise) {
            std::cerr << "[ProcessRequest] Invalid context" << std::endl;
            return;
        }

        HttpResponse response;

        try {
            // WalletMessageRouter 호출 (기존 그대로)
            auto protobuf_response = WalletMessageRouter::Instance().ProcessMessage(
                context->request.get()
            );

            if (protobuf_response) {
                response.status_code = 200;
                response.status_text = "OK";
                response.protobuf_message = std::move(protobuf_response);
            } else {
                response.status_code = 500;
                response.status_text = "Internal Server Error";
            }
        } catch (const std::exception& e) {
            std::cerr << "[ProcessRequest] Exception: " << e.what() << std::endl;
            response.status_code = 500;
            response.status_text = "Internal Server Error";
        }

        // promise에 결과 설정 (send_queue.Push 대신)
        try {
            context->promise->set_value(std::move(response));
        } catch (const std::exception& e) {
            std::cerr << "[ProcessRequest] Promise set_value failed: " 
                      << e.what() << std::endl;
        }
    }

} // namespace mpc_engine::coordinator::network::wallet_server