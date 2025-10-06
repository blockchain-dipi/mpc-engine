// src/node/network/src/NodeTcpServer.cpp
#include "node/network/include/NodeTcpServer.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/utils/firewall/KernelFirewall.hpp"
#include "common/utils/threading/ThreadUtils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

namespace mpc_engine::node::network
{
    NodeTcpServer::NodeTcpServer(const std::string& address, uint16_t port, size_t handler_threads)
    : bind_address(address), bind_port(port), num_handler_threads(handler_threads)
    {
        if (handler_threads == 0) {
            throw std::invalid_argument("handler_threads must be at least 1");
        }
    }

    NodeTcpServer::~NodeTcpServer() 
    {
        Stop();
    }

    bool NodeTcpServer::Initialize() 
    {
        if (is_initialized.load()) {
            return true;
        }

        // Create server socket
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET_VALUE) {
            std::cerr << "Failed to create server socket" << std::endl;
            return false;
        }

        SetSocketOptions(server_socket);

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(bind_port);
        
        if (inet_pton(AF_INET, bind_address.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid bind address: " << bind_address << std::endl;
            utils::CloseSocket(server_socket);
            return false;
        }

        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind to " << bind_address << ":" << bind_port << std::endl;
            utils::CloseSocket(server_socket);
            return false;
        }

        // Initialize thread pool
        handler_pool = std::make_unique<utils::ThreadPool<HandlerContext>>(num_handler_threads);
        
        // Initialize send queue (size: num_handlers * 5)
        send_queue = std::make_unique<utils::ThreadSafeQueue<NetworkMessage>>(
            num_handler_threads * 5
        );

        is_initialized = true;
        std::cout << "NodeTcpServer initialized with " << num_handler_threads << " handler threads" << std::endl;
        return true;
    }

    bool NodeTcpServer::Start() 
    {
        if (!is_initialized.load() || is_running.load()) {
            return false;
        }

        if (listen(server_socket, 1) < 0) {
            std::cerr << "Failed to listen on socket" << std::endl;
            return false;
        }

        // 커널 방화벽 설정 (옵션)
        if (enable_kernel_firewall) {
            if (!security_config.trusted_coordinator_ip.empty()) {
                std::cout << "[SECURITY] Configuring kernel-level firewall..." << std::endl;
                
                if (utils::KernelFirewall::ConfigureNodeFirewall(
                    bind_port, 
                    security_config.trusted_coordinator_ip
                )) {
                    std::cout << "[SECURITY] ✓ Kernel firewall active" << std::endl;
                    std::cout << "[SECURITY] ✓ SYN packets from untrusted IPs will be DROPped at kernel level" << std::endl;
                } else {
                    std::cerr << "[SECURITY] ⚠ Failed to configure kernel firewall" << std::endl;
                    std::cerr << "[SECURITY] ⚠ Falling back to application-level security only" << std::endl;
                }
            } else {
                std::cerr << "[SECURITY] ⚠ Kernel firewall enabled but no trusted IP set" << std::endl;
            }
        }

        is_running = true;
        accepting_connections = true;
        
        // Start Connection threads
        connection_thread = std::thread(&NodeTcpServer::ConnectionLoop, this);
        
        std::cout << "NodeTcpServer started on " << bind_address << ":" << bind_port << std::endl;
        return true;
    }

    void NodeTcpServer::Stop() 
    {
        if (!is_running.load()) {
            return;
        }

        std::cout << "Stopping NodeTcpServer..." << std::endl;
        is_running = false;

        // 🔹 1단계: 서버 소켓 종료 (accept 차단)
        if (server_socket != INVALID_SOCKET_VALUE) {
            shutdown(server_socket, SHUT_RDWR);
            utils::CloseSocket(server_socket);
            server_socket = INVALID_SOCKET_VALUE;
        }

        // 🔹 2단계: 커널 방화벽 규칙 제거
        if (enable_kernel_firewall) {
            std::cout << "[SECURITY] Removing kernel firewall rules..." << std::endl;
            utils::KernelFirewall::RemoveNodeFirewall(bind_port);
        }

        // 🔹 3단계: 기존 연결 강제 종료 (recv 차단)
        ForceCloseExistingConnection();

        // 🔹 4단계: ThreadPool Shutdown
        if (handler_pool) {
            std::cout << "[ThreadPool] Shutting down handlers..." << std::endl;
            handler_pool->Shutdown();
        }

        // 🔹 5단계: Send Queue Shutdown
        if (send_queue) {
            std::cout << "[Queue] Shutting down send queue..." << std::endl;
            send_queue->Shutdown();
        }

        // 🆕 6단계: 스레드 안전 종료 (타임아웃 적용)
        constexpr uint32_t THREAD_JOIN_TIMEOUT_MS = 5000;  // 5초
        
        std::cout << "[Threads] Waiting for threads to stop (timeout: " 
                  << THREAD_JOIN_TIMEOUT_MS << "ms)..." << std::endl;

        // Connection thread
        if (connection_thread.joinable()) {
            utils::JoinResult result = utils::JoinWithTimeout(connection_thread, THREAD_JOIN_TIMEOUT_MS);
            std::cout << "  Connection thread: " << utils::ToString(result) << std::endl;
            
            if (result == utils::JoinResult::TIMEOUT) {
                std::cerr << "  ⚠️  Connection thread did not stop in time!" << std::endl;
            }
        }

        // Receive thread
        if (receive_thread.joinable()) {
            utils::JoinResult result = utils::JoinWithTimeout(receive_thread, THREAD_JOIN_TIMEOUT_MS);
            std::cout << "  Receive thread: " << utils::ToString(result) << std::endl;
            
            if (result == utils::JoinResult::TIMEOUT) {
                std::cerr << "  ⚠️  Receive thread did not stop in time!" << std::endl;
            }
        }

        // Send thread
        if (send_thread.joinable()) {
            utils::JoinResult result = utils::JoinWithTimeout(send_thread, THREAD_JOIN_TIMEOUT_MS);
            std::cout << "  Send thread: " << utils::ToString(result) << std::endl;
            
            if (result == utils::JoinResult::TIMEOUT) {
                std::cerr << "  ⚠️  Send thread did not stop in time!" << std::endl;
            }
        }

        std::cout << "NodeTcpServer stopped" << std::endl;
    }

    bool NodeTcpServer::PrepareShutdown(uint32_t timeout_ms) 
    {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  Preparing Graceful Shutdown" << std::endl;
        std::cout << "========================================" << std::endl;

        std::cout << "[1/3] Stopping new connections..." << std::endl;
        StopAcceptingConnections();
        std::cout << "  ✓ No longer accepting new connections" << std::endl;

        std::cout << "[2/3] Waiting for pending requests..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        bool completed = false;

        while (true) {
            uint32_t pending = GetPendingRequests();
            if (pending == 0) {
                std::cout << "  ✓ All requests completed" << std::endl;
                completed = true;
                break;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();

            if (elapsed > timeout_ms) {
                std::cout << "  ⚠ Timeout: " << pending << " requests pending" << std::endl;
                break;
            }

            std::cout << "  Pending: " << pending << " (" << elapsed << "ms)" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        std::cout << "[3/3] Additional cleanup..." << std::endl;
        // TODO: 나중에 여기에 DB 커밋, 로그 플러시 등 추가
        std::cout << "  ✓ Cleanup complete" << std::endl;

        std::cout << "========================================" << std::endl;
        std::cout << (completed ? "  ✓ Ready for shutdown" : "  ⚠ Forced shutdown") << std::endl;
        std::cout << "========================================\n" << std::endl;

        return completed;
    }

    void NodeTcpServer::StopAcceptingConnections() 
    {
        accepting_connections = false;
    }

    uint32_t NodeTcpServer::GetPendingRequests() const 
    {
        uint32_t pending = 0;
        if (handler_pool) {
            pending += handler_pool->GetActiveTaskCount();
        }
        if (send_queue) {
            pending += send_queue->Size();
        }
        return pending;
    }

    void NodeTcpServer::ConnectionLoop() 
    {
        std::cout << "Connection thread started" << std::endl;
        std::cout << "Listening on " << bind_address << ":" << bind_port << std::endl;
        
        if (!security_config.trusted_coordinator_ip.empty()) {
            std::cout << "[SECURITY] Trusted Coordinator: " << security_config.trusted_coordinator_ip << std::endl;
        }
        
        while (is_running.load()) {
            if (!accepting_connections.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            socket_t client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);
            
            if (!is_running.load()) break;
            
            if (client_socket == INVALID_SOCKET_VALUE) {
                if (is_running.load()) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            uint16_t client_port = ntohs(client_addr.sin_port);

            // Security check
            if (!IsAuthorized(client_ip)) {
                std::cerr << "[SECURITY] Rejected connection from " << client_ip << ":" << client_port << std::endl;
                utils::CloseSocket(client_socket);
                continue;
            }

            // Close existing connection
            ForceCloseExistingConnection();

            std::cout << "[SECURITY] Accepted connection from " << client_ip << ":" << client_port << std::endl;
            HandleCoordinatorConnection(client_socket, client_ip, client_port);
        }
        
        std::cout << "Connection thread stopped" << std::endl;
    }

    void NodeTcpServer::HandleCoordinatorConnection(
        socket_t client_socket,
        const std::string& client_ip,
        uint16_t client_port
    ) {
        // Create connection info
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            coordinator_connection = std::make_unique<NodeConnectionInfo>();
            coordinator_connection->Initialize(client_socket, client_ip, client_port);
        }

        // Notify connected
        if (connected_handler) {
            connected_handler(*coordinator_connection);
        }

        // Start receive and send threads
        receive_thread = std::thread(&NodeTcpServer::ReceiveLoop, this);
        send_thread = std::thread(&NodeTcpServer::SendLoop, this);

        // Wait for threads to complete
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
        if (send_thread.joinable()) {
            send_thread.join();
        }

        // Connection closed
        std::unique_ptr<NodeConnectionInfo> info_copy;
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            if (coordinator_connection) {
                coordinator_connection->status = ConnectionStatus::DISCONNECTED;
                info_copy = std::make_unique<NodeConnectionInfo>(*coordinator_connection);

                utils::CloseSocket(coordinator_connection->coordinator_socket);
                coordinator_connection.reset();
            }
        }
        // 콜백 호출 (lock 밖, 복사본 사용)
        if (info_copy && disconnected_handler) {
            disconnected_handler(*info_copy);
        }
    }

    void NodeTcpServer::ReceiveLoop()
    {
        using namespace protocol::coordinator_node;
    
        std::cout << "Receive thread started" << std::endl;
        
        socket_t sock = INVALID_SOCKET_VALUE;
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            if (coordinator_connection) {
                sock = coordinator_connection->coordinator_socket;
            }
        }
        
        if (sock == INVALID_SOCKET_VALUE) {
            std::cerr << "[ERROR] Invalid socket in ReceiveLoop" << std::endl;
            return;
        }
    
        if (!message_handler) {
            std::cerr << "[ERROR] message_handler is null, cannot process messages" << std::endl;
            return;
        }
    
        while (is_running.load() && HasActiveConnection()) {
            NetworkMessage request;
            
            if (!ReceiveMessage(sock, request)) {
                std::cerr << "[INFO] Connection lost or receive failed" << std::endl;
                break;
            }
        
            total_messages_received++;
            
            {
                std::lock_guard<std::mutex> lock(connection_mutex);
                if (coordinator_connection) {
                    coordinator_connection->last_activity_time = utils::GetCurrentTimeMs();
                    coordinator_connection->total_requests_handled++;
                }
            }
        
            // ✅ unique_ptr로 생성
            auto context = std::make_unique<HandlerContext>(
                request, 
                message_handler, 
                send_queue.get()
            );
            
            try {
                // ✅ SubmitOwned로 소유권 이전
                handler_pool->SubmitOwned(ProcessMessage, std::move(context));
                
            } catch (const std::runtime_error& e) {
                // ✅ context는 이미 move됨 → 메모리 누수 없음
                std::cerr << "[ERROR] Failed to submit task (pool stopped): " << e.what() << std::endl;
                
                NetworkMessage error_response = CreateErrorResponse(
                    request.header.message_type,
                    "Server shutting down"
                );
                
                utils::QueueResult result = send_queue->TryPush(
                    error_response, 
                    std::chrono::milliseconds(100)
                );
                
                if (result != utils::QueueResult::SUCCESS) {
                    std::cerr << "[ERROR] Failed to push error response: " 
                              << utils::ToString(result) << std::endl;
                }
                
                break;
                
            } catch (const std::exception& e) {
                // ✅ context는 이미 move됨 → 메모리 누수 없음
                std::cerr << "[ERROR] Failed to submit task: " << e.what() << std::endl;
                
                NetworkMessage error_response = CreateErrorResponse(
                    request.header.message_type,
                    "Server busy"
                );
                
                utils::QueueResult result = send_queue->TryPush(
                    error_response, 
                    std::chrono::milliseconds(100)
                );
                
                if (result != utils::QueueResult::SUCCESS) {
                    std::cerr << "[ERROR] Failed to push error response: " 
                              << utils::ToString(result) << std::endl;
                }
                
                handler_errors++;
            }
        }
        
        std::cout << "Receive thread stopped" << std::endl;
    }

    void NodeTcpServer::SendLoop()
    {
        std::cout << "Send thread started" << std::endl;
        
        socket_t sock = INVALID_SOCKET_VALUE;
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            if (coordinator_connection) {
                sock = coordinator_connection->coordinator_socket;
            }
        }
        
        if (sock == INVALID_SOCKET_VALUE) {
            std::cerr << "Invalid socket in SendLoop" << std::endl;
            return;
        }

        while (is_running.load() && HasActiveConnection()) {
            NetworkMessage response;
            
            // 🆕 QueueResult 사용
            utils::QueueResult result = send_queue->Pop(response);
            
            if (result == utils::QueueResult::SHUTDOWN) {
                std::cout << "[INFO] Send queue shutdown" << std::endl;
                break;
            }
            
            if (result != utils::QueueResult::SUCCESS) {
                std::cerr << "[ERROR] Pop from send queue failed: " 
                          << utils::ToString(result) << std::endl;
                break;
            }

            if (SendMessage(sock, response)) {
                total_messages_sent++;
                
                std::lock_guard<std::mutex> lock(connection_mutex);
                if (coordinator_connection) {
                    coordinator_connection->successful_requests++;
                }
            } else {
                std::cerr << "Failed to send response" << std::endl;
                break;
            }
        }
        
        std::cout << "Send thread stopped" << std::endl;
    }

    void NodeTcpServer::ProcessMessage(HandlerContext* context)
    {
        using namespace protocol::coordinator_node;

        std::unique_ptr<HandlerContext> ctx(context);

        if (!ctx) {
            std::cerr << "[ERROR] ProcessMessage received null context" << std::endl;
            return;
        }

        NetworkMessage response;
        uint64_t request_id = ctx->request.header.request_id;  // ✅ 미리 저장

        try {
            // 검증
            ValidationResult validation = ctx->request.Validate();
            if (validation != ValidationResult::OK) {
                std::cerr << "[ERROR] Invalid request in handler: " << ToString(validation) << std::endl;

                response = CreateErrorResponse(
                    ctx->request.header.message_type,
                    std::string("Invalid request: ") + ToString(validation)
                );
                response.header.request_id = request_id;

                utils::QueueResult result = ctx->send_queue->TryPush(
                    response, 
                    std::chrono::milliseconds(100)
                );

                if (result != utils::QueueResult::SUCCESS) {
                    std::cerr << "[ERROR] Failed to push response: " 
                              << utils::ToString(result) << std::endl;
                }

                return;
            }

            if (!ctx->handler) {
                std::cerr << "[ERROR] Handler is null" << std::endl;

                response = CreateErrorResponse(
                    ctx->request.header.message_type,
                    "Handler not configured"
                );
                response.header.request_id = request_id;

                utils::QueueResult result = ctx->send_queue->TryPush(
                    response, 
                    std::chrono::milliseconds(100)
                );

                if (result != utils::QueueResult::SUCCESS) {
                    std::cerr << "[ERROR] Failed to push response: " 
                              << utils::ToString(result) << std::endl;
                }

                return;
            }

            // 핸들러 호출
            response = ctx->handler(ctx->request);

            // ✅ Request ID 복사 (가장 중요!)
            response.header.request_id = request_id;

            if (response.Validate() != ValidationResult::OK) {
                std::cerr << "[ERROR] Handler generated invalid response" << std::endl;

                response = CreateErrorResponse(
                    ctx->request.header.message_type,
                    "Handler generated invalid response"
                );
                response.header.request_id = request_id;
            }

        } catch (const std::bad_alloc& e) {
            std::cerr << "[ERROR] Memory allocation error in handler: " << e.what() << std::endl;
            response = CreateErrorResponse(
                ctx->request.header.message_type,
                "Server memory error"
            );
            response.header.request_id = request_id;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Handler exception: " << e.what() << std::endl;
            response = CreateErrorResponse(
                ctx->request.header.message_type,
                std::string("Handler error: ") + e.what()
            );
            response.header.request_id = request_id;

        } catch (...) {
            std::cerr << "[ERROR] Unknown handler exception" << std::endl;
            response = CreateErrorResponse(
                ctx->request.header.message_type,
                "Unknown server error"
            );
            response.header.request_id = request_id;
        }

        // 응답 전송
        utils::QueueResult result = ctx->send_queue->TryPush(
            response, 
            std::chrono::milliseconds(1000)
        );

        if (result == utils::QueueResult::TIMEOUT) {
            std::cerr << "[ERROR] Failed to push response to send queue: timeout after 1000ms" << std::endl;
        } else if (result == utils::QueueResult::SHUTDOWN) {
            std::cerr << "[INFO] Send queue is shutdown, discarding response" << std::endl;
        } else if (result != utils::QueueResult::SUCCESS) {
            std::cerr << "[ERROR] Failed to push response: " << utils::ToString(result) << std::endl;
        }
    }

    bool NodeTcpServer::IsAuthorized(const std::string& client_ip)
    {
        return security_config.IsAllowed(client_ip);
    }

    void NodeTcpServer::ForceCloseExistingConnection()
    {
        std::lock_guard<std::mutex> lock(connection_mutex);
        
        if (coordinator_connection) {
            std::cout << "Closing existing connection" << std::endl;
            
            if (coordinator_connection->coordinator_socket != INVALID_SOCKET_VALUE) {
                utils::CloseSocket(coordinator_connection->coordinator_socket);
            }
            
            coordinator_connection.reset();
        }
    }

    void NodeTcpServer::SetSocketOptions(socket_t sock)
    {
        utils::SetSocketReuseAddr(sock);
        utils::SetSocketNoDelay(sock);
        
        utils::KeepAliveConfig keepalive;
        keepalive.enabled = true;
        keepalive.idle_seconds = 10;
        keepalive.interval_seconds = 5;
        keepalive.probe_count = 3;
        utils::SetSocketKeepAlive(sock, keepalive);
        
        utils::SetSocketRecvTimeout(sock, 30000);
        utils::SetSocketBufferSize(sock, 64 * 1024, 64 * 1024);
    }

    bool NodeTcpServer::SendMessage(socket_t sock, const NetworkMessage& outMessage)
    {
        // ✅ SendExact 사용
        size_t bytes_sent = 0;
        
        // 1. 헤더 전송
        utils::SocketIOResult result = utils::SendExact(
            sock, 
            &outMessage.header, 
            sizeof(MessageHeader), 
            &bytes_sent
        );
        
        if (result != utils::SocketIOResult::SUCCESS) {
            std::cerr << "[ERROR] Failed to send header: " 
                      << utils::ToString(result) << std::endl;
            return false;
        }

        // 2. Body 전송 (있으면)
        if (outMessage.header.body_length > 0) {
            result = utils::SendExact(
                sock, 
                outMessage.body.data(), 
                outMessage.body.size(), 
                &bytes_sent
            );
            
            if (result != utils::SocketIOResult::SUCCESS) {
                std::cerr << "[ERROR] Failed to send body: " 
                          << utils::ToString(result) 
                          << " (sent: " << bytes_sent << "/" << outMessage.body.size() << " bytes)" 
                          << std::endl;
                return false;
            }
        }

        return true;
    }

    bool NodeTcpServer::ReceiveMessage(socket_t sock, NetworkMessage& outMessage)
    {
        size_t bytes_received = 0;
        
        // ✅ 1단계: 헤더 수신 (ReceiveExact 사용)
        utils::SocketIOResult result = utils::ReceiveExact(
            sock, 
            &outMessage.header, 
            sizeof(MessageHeader), 
            &bytes_received
        );
        
        if (result != utils::SocketIOResult::SUCCESS) {
            if (result == utils::SocketIOResult::CONNECTION_CLOSED) {
                std::cout << "[INFO] Connection closed gracefully" << std::endl;
            } else {
                std::cerr << "[SECURITY] Header receive failed: " 
                          << utils::ToString(result) 
                          << " (received: " << bytes_received << "/" << sizeof(MessageHeader) << " bytes)" 
                          << std::endl;
            }
            return false;
        }
    
        // ✅ 2단계: 헤더 기본 검증
        ValidationResult validation = outMessage.header.ValidateBasic();
        if (validation != ValidationResult::OK) {
            std::cerr << "[SECURITY] Header validation failed: " << ToString(validation) << std::endl;
            std::cerr << "[SECURITY]   Magic: 0x" << std::hex << outMessage.header.magic << std::dec << std::endl;
            std::cerr << "[SECURITY]   Version: " << outMessage.header.version << std::endl;
            std::cerr << "[SECURITY]   Body length: " << outMessage.header.body_length << std::endl;
            return false;
        }
    
        // ✅ 3단계: 메시지 타입 검증
        if (!outMessage.header.IsValidMessageType()) {
            std::cerr << "[SECURITY] Invalid message type: " << outMessage.header.message_type << std::endl;
            return false;
        }
    
        // ✅ 4단계: Body 수신 (크기가 이미 검증됨)
        if (outMessage.header.body_length > 0) {
            try {
                outMessage.body.resize(outMessage.header.body_length);
            } catch (const std::bad_alloc& e) {
                std::cerr << "[SECURITY] Memory allocation failed for body: " << e.what() << std::endl;
                return false;
            }
        
            bytes_received = 0;
            result = utils::ReceiveExact(
                sock, 
                outMessage.body.data(), 
                outMessage.header.body_length, 
                &bytes_received
            );
            
            if (result != utils::SocketIOResult::SUCCESS) {
                std::cerr << "[SECURITY] Body receive failed: " 
                          << utils::ToString(result)
                          << " (received: " << bytes_received 
                          << "/" << outMessage.header.body_length << " bytes)" << std::endl;
                return false;
            }
        }
    
        // ✅ 5단계: 전체 메시지 검증 (checksum 포함)
        validation = outMessage.Validate();
        if (validation != ValidationResult::OK) {
            std::cerr << "[SECURITY] Message validation failed: " << ToString(validation) << std::endl;
            return false;
        }
    
        // ✅ 모든 검증 통과
        return true;
    }

    void NodeTcpServer::SetTrustedCoordinator(const std::string& ip)
    {
        security_config.trusted_coordinator_ip = ip;
        std::cout << "[SECURITY] Trusted Coordinator set to: " << ip << std::endl;
    }

    bool NodeTcpServer::HasActiveConnection() const
    {
        std::lock_guard<std::mutex> lock(connection_mutex);
        return coordinator_connection && coordinator_connection->IsActive();
    }

    void NodeTcpServer::SetMessageHandler(MessageHandler handler)
    {
        message_handler = handler;
    }

    void NodeTcpServer::SetConnectedHandler(ConnectionHandler handler)
    {
        connected_handler = handler;
    }

    void NodeTcpServer::SetDisconnectedHandler(ConnectionHandler handler)
    {
        disconnected_handler = handler;
    }

    NodeTcpServer::ServerStats NodeTcpServer::GetStats() const
    {
        ServerStats stats;
        stats.messages_received = total_messages_received.load();
        stats.messages_sent = total_messages_sent.load();
        stats.messages_processed = total_messages_processed.load();
        stats.handler_errors = handler_errors.load();
        stats.pending_send_queue = send_queue ? send_queue->Size() : 0;
        stats.active_handlers = handler_pool ? handler_pool->GetActiveTaskCount() : 0;
        return stats;
    }

    bool NodeTcpServer::IsRunning() const
    {
        return is_running.load();
    }
    
    // 에러 응답 생성 헬퍼
    NetworkMessage NodeTcpServer::CreateErrorResponse(uint16_t original_message_type, const std::string& error_message)
    {
        std::string payload = "success=false|error=" + error_message;
        NetworkMessage error_msg(original_message_type, payload);
        return error_msg;
    }
}