// src/node/network/src/NodeTcpServer.cpp
#include "node/network/include/NodeTcpServer.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/utils/firewall/KernelFirewall.hpp"
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

        // Close server socket (will unblock accept())
        if (server_socket != INVALID_SOCKET_VALUE) {
            utils::CloseSocket(server_socket);
            server_socket = INVALID_SOCKET_VALUE;
        }

        // 커널 방화벽 규칙 제거
        if (enable_kernel_firewall) {
            std::cout << "[SECURITY] Removing kernel firewall rules..." << std::endl;
            utils::KernelFirewall::RemoveNodeFirewall(bind_port);
        }

        // Close connection (will unblock recv())
        ForceCloseExistingConnection();

        // Shutdown thread pool
        if (handler_pool) {
            handler_pool->Shutdown();
        }

        // Shutdown send queue
        if (send_queue) {
            send_queue->Shutdown();
        }

        // Join threads
        if (connection_thread.joinable()) {
            connection_thread.join();
        }
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
        if (send_thread.joinable()) {
            send_thread.join();
        }

        std::cout << "NodeTcpServer stopped" << std::endl;
    }

    void NodeTcpServer::ConnectionLoop() 
    {
        std::cout << "Connection thread started" << std::endl;
        std::cout << "Listening on " << bind_address << ":" << bind_port << std::endl;
        
        if (!security_config.trusted_coordinator_ip.empty()) {
            std::cout << "[SECURITY] Trusted Coordinator: " << security_config.trusted_coordinator_ip << std::endl;
        }
        
        while (is_running.load()) {
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
    
        // Handler 존재 확인
        if (!message_handler) {
            std::cerr << "[ERROR] message_handler is null, cannot process messages" << std::endl;
            return;
        }
    
        while (is_running.load() && HasActiveConnection()) {
            NetworkMessage request;
            
            // 메시지 수신 (검증 포함)
            if (!ReceiveMessage(sock, request)) {
                std::cerr << "[INFO] Connection lost or receive failed" << std::endl;
                break;
            }
        
            total_messages_received++;
            
            // 연결 정보 업데이트
            {
                std::lock_guard<std::mutex> lock(connection_mutex);
                if (coordinator_connection) {
                    coordinator_connection->last_activity_time = utils::GetCurrentTimeMs();
                    coordinator_connection->total_requests_handled++;
                }
            }
        
            // Handler pool에 제출
            auto* context = new HandlerContext(request, message_handler, send_queue.get());
            
            try {
                handler_pool->Submit(ProcessMessage, context);
                
            } catch (const std::runtime_error& e) {
                // ThreadPool stopped
                std::cerr << "[ERROR] Failed to submit task (pool stopped): " << e.what() << std::endl;
                
                // 에러 응답 생성 후 직접 전송 시도
                NetworkMessage error_response = CreateErrorResponse(
                    request.header.message_type,
                    "Server shutting down"
                );
                
                send_queue->TryPush(error_response, std::chrono::milliseconds(100));
                delete context;
                break;
                
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Failed to submit task: " << e.what() << std::endl;
                
                // 에러 응답
                NetworkMessage error_response = CreateErrorResponse(
                    request.header.message_type,
                    "Server busy"
                );
                
                send_queue->TryPush(error_response, std::chrono::milliseconds(100));
                delete context;
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
            
            // Block until response available
            if (!send_queue->Pop(response)) {
                // Queue shutdown
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

        if (!context) {
            std::cerr << "[ERROR] ProcessMessage received null context" << std::endl;
            return;
        }

        NetworkMessage response;

        try {
            // 1. Request 메시지 재검증 (방어적)
            ValidationResult validation = context->request.Validate();
            if (validation != ValidationResult::OK) {
                std::cerr << "[ERROR] Invalid request in handler: " << ToString(validation) << std::endl;

                // 에러 응답 생성
                response = CreateErrorResponse(
                    context->request.header.message_type,
                    std::string("Invalid request: ") + ToString(validation)
                );

                // Send queue에 추가
                context->send_queue->TryPush(response, std::chrono::milliseconds(100));
                delete context;
                return;
            }

            // 2. Handler 호출
            if (!context->handler) {
                std::cerr << "[ERROR] Handler is null" << std::endl;

                response = CreateErrorResponse(
                    context->request.header.message_type,
                    "Handler not configured"
                );

                context->send_queue->TryPush(response, std::chrono::milliseconds(100));
                delete context;
                return;
            }

            response = context->handler(context->request);

            // 3. Response 검증
            if (response.Validate() != ValidationResult::OK) {
                std::cerr << "[ERROR] Handler generated invalid response" << std::endl;

                // 새로운 에러 응답 생성
                response = CreateErrorResponse(
                    context->request.header.message_type,
                    "Handler generated invalid response"
                );
            }

        } catch (const std::bad_alloc& e) {
            std::cerr << "[ERROR] Memory allocation error in handler: " << e.what() << std::endl;
            response = CreateErrorResponse(
                context->request.header.message_type,
                "Server memory error"
            );

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Handler exception: " << e.what() << std::endl;
            response = CreateErrorResponse(
                context->request.header.message_type,
                std::string("Handler error: ") + e.what()
            );

        } catch (...) {
            std::cerr << "[ERROR] Unknown handler exception" << std::endl;
            response = CreateErrorResponse(
                context->request.header.message_type,
                "Unknown server error"
            );
        }

        // 4. Send queue에 응답 추가
        if (!context->send_queue->TryPush(response, std::chrono::milliseconds(1000))) {
            std::cerr << "[ERROR] Failed to push response to send queue (timeout)" << std::endl;
        }

        delete context;
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
        if (send(sock, &outMessage.header, sizeof(MessageHeader), MSG_NOSIGNAL) <= 0) {
            return false;
        }

        if (outMessage.header.body_length > 0) {
            if (send(sock, outMessage.body.data(), outMessage.body.size(), MSG_NOSIGNAL) <= 0) {
                return false;
            }
        }

        return true;
    }

    bool NodeTcpServer::ReceiveMessage(socket_t sock, NetworkMessage& outMessage)
    {
        // 1단계: 헤더 수신
        ssize_t received = recv(sock, &outMessage.header, sizeof(MessageHeader), MSG_WAITALL);
        if (received != sizeof(MessageHeader)) {
            std::cerr << "[SECURITY] Incomplete header received: " << received << " bytes" << std::endl;
            return false;
        }
    
        // 2단계: 헤더 기본 검증
        ValidationResult result = outMessage.header.ValidateBasic();
        if (result != ValidationResult::OK) {
            std::cerr << "[SECURITY] Header validation failed: " << ToString(result) << std::endl;
            std::cerr << "[SECURITY]   Magic: 0x" << std::hex << outMessage.header.magic << std::dec << std::endl;
            std::cerr << "[SECURITY]   Version: " << outMessage.header.version << std::endl;
            std::cerr << "[SECURITY]   Body length: " << outMessage.header.body_length << std::endl;
            
            // 악의적 연결 차단 고려
            // CloseConnection();
            return false;
        }
    
        // 3단계: 메시지 타입 검증
        if (!outMessage.header.IsValidMessageType()) {
            std::cerr << "[SECURITY] Invalid message type: " << outMessage.header.message_type << std::endl;
            return false;
        }
    
        // 4단계: Body 수신 (크기가 이미 검증됨)
        if (outMessage.header.body_length > 0) {
            try {
                outMessage.body.resize(outMessage.header.body_length);
            } catch (const std::bad_alloc& e) {
                std::cerr << "[SECURITY] Memory allocation failed for body: " << e.what() << std::endl;
                return false;
            }
        
            received = recv(sock, outMessage.body.data(), outMessage.header.body_length, MSG_WAITALL);
            if (received != static_cast<ssize_t>(outMessage.header.body_length)) {
                std::cerr << "[SECURITY] Incomplete body received: " << received 
                          << " / " << outMessage.header.body_length << " bytes" << std::endl;
                return false;
            }
        }
    
        // 5단계: 전체 메시지 검증 (checksum 포함)
        result = outMessage.Validate();
        if (result != ValidationResult::OK) {
            std::cerr << "[SECURITY] Message validation failed: " << ToString(result) << std::endl;
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