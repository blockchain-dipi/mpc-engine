// src/coordinator/network/wallet_server/src/WalletConnection.cpp
#include "coordinator/network/wallet_server/include/WalletConnection.hpp"
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>

namespace mpc_engine::coordinator::network::wallet_server
{
    using namespace mpc_engine::coordinator::handlers::wallet;

    WalletConnection::WalletConnection(TlsConnection&& tls_conn, 
                                      ThreadPool<WalletHandlerContext>* pool)
        : tls_connection(std::move(tls_conn))
        , handler_pool(pool)
        , send_queue(100)
    {
        last_activity_time.store(utils::GetCurrentTimeMs());
    }

    WalletConnection::~WalletConnection() 
    {
        Stop();
    }

    void WalletConnection::Start() 
    {
        if (active.exchange(true)) {
            return;
        }

        std::cout << "[WalletConnection] Starting..." << std::endl;

        receive_thread = std::thread(&WalletConnection::ReceiveLoop, this);
        send_thread = std::thread(&WalletConnection::SendLoop, this);
    }

    void WalletConnection::Stop() 
    {
        if (!active.exchange(false)) {
            return;
        }

        std::cout << "[WalletConnection] Stopping..." << std::endl;

        send_queue.Shutdown();
        tls_connection.Shutdown();

        if (receive_thread.joinable()) {
            receive_thread.join();
        }

        if (send_thread.joinable()) {
            send_thread.join();
        }

        std::cout << "[WalletConnection] Stopped" << std::endl;
    }

    uint64_t WalletConnection::GetIdleTime() const 
    {
        uint64_t current = utils::GetCurrentTimeMs();
        uint64_t last = last_activity_time.load();
        return (current > last) ? (current - last) : 0;
    }

    void WalletConnection::ReceiveLoop() 
    {
        std::cout << "[WalletConnection] Receive thread started" << std::endl;

        while (active.load()) 
        {
            HttpRequest request;
            TlsError err = HttpParser::ReceiveRequest(tls_connection, request);

            if (err == TlsError::CONNECTION_CLOSED) {
                std::cout << "[WalletConnection] Client closed connection" << std::endl;
                break;
            }

            if (err != TlsError::NONE) {
                std::cerr << "[WalletConnection] Receive error" << std::endl;
                break;
            }

            last_activity_time.store(utils::GetCurrentTimeMs());

            if (!request.protobuf_message) {
                std::cerr << "[WalletConnection] No protobuf message" << std::endl;
                continue;
            }

            std::cout << "[WalletConnection] Received request: " 
                      << request.method << " " << request.path << std::endl;

            auto req_ptr = std::make_unique<HttpRequest>(std::move(request));
            auto context = std::make_unique<WalletHandlerContext>(std::move(req_ptr), &send_queue);
            
            try {
                handler_pool->SubmitOwned(ProcessRequest, std::move(context));
            } catch (const std::exception& e) {
                std::cerr << "[WalletConnection] Failed to submit task: " << e.what() << std::endl;
            }
        }

        std::cout << "[WalletConnection] Receive thread stopped" << std::endl;
    }

    void WalletConnection::SendLoop() 
    {
        std::cout << "[WalletConnection] Send thread started" << std::endl;

        while (active.load()) 
        {
            HttpResponse response;
            QueueResult result = send_queue.Pop(response);

            if (result == QueueResult::SHUTDOWN) {
                break;
            }

            if (result != QueueResult::SUCCESS) {
                continue;
            }

            TlsError err = HttpParser::SendResponse(tls_connection, response);

            if (err != TlsError::NONE) {
                std::cerr << "[WalletConnection] Send error" << std::endl;
                break;
            }

            std::cout << "[WalletConnection] Sent response: " << response.status_code << std::endl;

            last_activity_time.store(utils::GetCurrentTimeMs());
        }

        std::cout << "[WalletConnection] Send thread stopped" << std::endl;
    }

    void WalletConnection::ProcessRequest(WalletHandlerContext* context) 
    {
        if (!context || !context->request || !context->send_queue) {
            std::cerr << "[WalletConnection] Invalid handler context" << std::endl;
            return;
        }

        HttpResponse response;

        try 
        {
            auto protobuf_response = WalletMessageRouter::Instance().ProcessMessage(
                context->request->protobuf_message.get()
            );

            if (protobuf_response) {
                response.status_code = 200;
                response.status_text = "OK";
                response.protobuf_message = std::move(protobuf_response);
            } else {
                response.status_code = 500;
                response.status_text = "Internal Server Error";
            }
        } 
        catch (const std::exception& e) 
        {
            std::cerr << "[WalletConnection] Handler exception: " << e.what() << std::endl;
            response.status_code = 500;
            response.status_text = "Internal Server Error";
        }

        context->send_queue->Push(std::move(response));
    }

} // namespace mpc_engine::coordinator::network::wallet_server