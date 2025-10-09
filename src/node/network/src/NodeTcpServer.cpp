// src/node/network/src/NodeTcpServer.cpp
#include "node/network/include/NodeTcpServer.hpp"
#include "common/config/ConfigManager.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/utils/firewall/KernelFirewall.hpp"
#include "common/utils/threading/ThreadUtils.hpp"
#include "common/kms/include/KMSManager.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>

namespace mpc_engine::node::network
{
    using namespace mpc_engine::config;
    using namespace mpc_engine::kms;

    constexpr uint32_t THREAD_JOIN_TIMEOUT_MS = 5000;  // 5초

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
    
    bool NodeTcpServer::InitializeTlsContext(const std::string& certificate_path, const std::string& private_key_id)
    {
        tls_context = std::make_unique<TlsContext>();

        TlsConfig config = TlsConfig::CreateSecureServerConfig();        
        if (!tls_context->Initialize(config)) {
            return false;
        }

        auto& kms = KMSManager::Instance();
        try {
            // 1. CA 인증서 로드 (클라이언트 인증서 검증용) ← 추가 필요
            std::string tls_ca = ConfigManager::Instance().GetString("TLS_KMS_CA_KEY_ID");
            std::string ca_pem = kms.GetSecret(tls_ca);
            if (ca_pem.empty()) {
                std::cerr << "[NodeTcpServer] Failed to load CA certificate from KMS" << std::endl;
                return false;
            }

            if (!tls_context->LoadCA(ca_pem)) {
                std::cerr << "[NodeTcpServer] Failed to load CA certificate to context" << std::endl;
                return false;
            }

            // 2. 서버 인증서 로드 (기존 코드)
            std::ifstream cert_file(certificate_path);
            if (!cert_file) {
                return false;
            }
            std::string certificate_pem((std::istreambuf_iterator<char>(cert_file)), std::istreambuf_iterator<char>());
            std::string private_key_pem = kms.GetSecret(private_key_id);

            if (certificate_pem.empty() || private_key_pem.empty()) {
                return false;
            }

            CertificateData cert_data;
            cert_data.certificate_pem = certificate_pem;
            cert_data.private_key_pem = private_key_pem;

            return tls_context->LoadCertificate(cert_data);

        } catch (const std::exception& e) {
            return false;
        }
    }

    bool NodeTcpServer::Initialize(const std::string& certificate_path, const std::string& private_key_id)
    {
        if (is_initialized.load()) {
            return true;
        }

        std::cout << "Initializing NodeTcpServer..." << std::endl;
        if (!InitializeTlsContext(certificate_path, private_key_id)) {
            std::cerr << "Failed to initialize TLS" << std::endl;
            return false;
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
        
        // Initialize send queue
        uint16_t handler_queue_size = Config::GetUInt16("NODE_SEND_QUEUE_SIZE_PER_HANDLER_THREAD");
        send_queue = std::make_unique<utils::ThreadSafeQueue<NetworkMessage>>(
            num_handler_threads * handler_queue_size
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

        // 6단계: 스레드 안전 종료 (타임아웃 적용)
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

    void NodeTcpServer::HandleCoordinatorConnection(socket_t client_socket, const std::string& client_ip, uint16_t client_port) 
    {
        // TLS Connection 생성 및 핸드셰이크
        auto tls_connection = std::make_unique<TlsConnection>();

        TlsConnectionConfig tls_config;
        tls_config.handshake_timeout_ms = 10000;

        if (!tls_connection->AcceptServer(*tls_context, client_socket, tls_config)) {
            utils::CloseSocket(client_socket);
            return;
        }

        if (!tls_connection->DoHandshake()) {
            utils::CloseSocket(client_socket);
            return;
        }

        // NodeConnectionInfo 생성 (TLS Connection 포함)
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            coordinator_connection = std::make_unique<NodeConnectionInfo>();
            coordinator_connection->InitializeWithTls(client_ip, client_port, std::move(tls_connection));
        }

        // connected_handler 호출
        if (connected_handler) {
            connected_handler(*coordinator_connection);
        }

        // 스레드 시작
        receive_thread = std::thread(&NodeTcpServer::ReceiveLoop, this);
        send_thread = std::thread(&NodeTcpServer::SendLoop, this);

        // 연결 종료 처리 (TLS Close 추가)
        NodeConnectionInfo::DisconnectionInfo disconnect_info;
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            if (coordinator_connection) {
                // 종료 전 정보 백업
                disconnect_info = coordinator_connection->GetDisconnectionInfo();
                
                // 안전한 종료
                coordinator_connection->Disconnect();
                coordinator_connection.reset();
            }
        }
    
        // 콜백 호출 (나중에 로깅, 통계, 재연결 로직 등 추가 가능)
        if (disconnected_handler && !disconnect_info.coordinator_address.empty()) {
            disconnected_handler(disconnect_info);
        }
    }

    void NodeTcpServer::ReceiveLoop()
    {
        std::cout << "Receive thread started" << std::endl;

        if (!message_handler) {
            std::cerr << "[ERROR] message_handler is null, cannot process messages" << std::endl;
            return;
        }

        while (is_running.load() && HasActiveConnection()) {
            NetworkMessage request;

            // socket 파라미터 제거 - ReceiveMessage 내부에서 TLS Connection 가져옴
            if (!ReceiveMessage(request)) {  // socket 파라미터는 더미값
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
        
            auto context = std::make_unique<HandlerContext>(
                request, 
                message_handler, 
                send_queue.get()
            );

            try {
                handler_pool->SubmitOwned(ProcessMessage, std::move(context));

            } catch (const std::runtime_error& e) {
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

        while (is_running.load() && HasActiveConnection()) {
            NetworkMessage outMessage;
            utils::QueueResult result = send_queue->Pop(outMessage);
            if (result != utils::QueueResult::SUCCESS) {
                continue;
            }
        
            // socket 파라미터 제거 - SendMessage 내부에서 TLS Connection 가져옴
            if (!SendMessage(outMessage)) {  // socket 파라미터는 더미값
                std::cerr << "[INFO] Connection lost or send failed" << std::endl;
                break;
            }
        
            total_messages_sent++;
        
            {
                std::lock_guard<std::mutex> lock(connection_mutex);
                if (coordinator_connection) {
                    coordinator_connection->last_activity_time = utils::GetCurrentTimeMs();
                    coordinator_connection->total_responses_sent++;
                }
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

    bool NodeTcpServer::SendMessage(const NetworkMessage& outMessage)
    {
        // TLS Connection 획득
        TlsConnection* tls_conn = nullptr;
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            if (coordinator_connection) {
                tls_conn = &coordinator_connection->GetTlsConnection();
            }
        }

        if (!tls_conn) {
            return false;
        }

        // 기존 utils::SendExact 로직을 TLS로 교체
        TlsError error = tls_conn->WriteExact(&outMessage.header, sizeof(MessageHeader));
        if (error != TlsError::NONE) {
            return false;
        }

        if (outMessage.header.body_length > 0) {
            error = tls_conn->WriteExact(outMessage.body.data(), outMessage.body.size());
            if (error != TlsError::NONE) {
                return false;
            }
        }

        return true;
    }

    bool NodeTcpServer::ReceiveMessage(NetworkMessage& outMessage)
    {
        // TLS Connection 획득
        TlsConnection* tls_conn = nullptr;
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            if (coordinator_connection) {
                tls_conn = &coordinator_connection->GetTlsConnection();
            }
        }

        if (!tls_conn) {
            return false;
        }

        // 기존 utils::ReceiveExact 로직을 TLS로 교체
        TlsError error = tls_conn->ReadExact(&outMessage.header, sizeof(MessageHeader));
        if (error != TlsError::NONE) {
            if (error == TlsError::CONNECTION_CLOSED) {
                std::cout << "[INFO] Connection closed gracefully" << std::endl;
            }
            return false;
        }

        // 기존 검증 로직 그대로 유지
        ValidationResult validation = outMessage.header.ValidateBasic();
        if (validation != ValidationResult::OK) {
            std::cerr << "[SECURITY] Header validation failed: " << ToString(validation) << std::endl;
            std::cerr << "[SECURITY]   Magic: 0x" << std::hex << outMessage.header.magic << std::dec << std::endl;
            std::cerr << "[SECURITY]   Version: " << outMessage.header.version << std::endl;
            std::cerr << "[SECURITY]   Body length: " << outMessage.header.body_length << std::endl;
            return false;
        }

        if (!outMessage.header.IsValidMessageType()) {
            std::cerr << "[SECURITY] Invalid message type: " << outMessage.header.message_type << std::endl;
            return false;
        }

        if (outMessage.header.body_length > 0) {
            try {
                outMessage.body.resize(outMessage.header.body_length);
            } catch (const std::bad_alloc& e) {
                std::cerr << "[SECURITY] Memory allocation failed for body: " << e.what() << std::endl;
                return false;
            }
        
            error = tls_conn->ReadExact(outMessage.body.data(), outMessage.header.body_length);
            if (error != TlsError::NONE) {
                return false;
            }
        }

        validation = outMessage.Validate();
        if (validation != ValidationResult::OK) {
            std::cerr << "[SECURITY] Message validation failed: " << ToString(validation) << std::endl;
            return false;
        }

        return true;
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
            
            if (coordinator_connection->tls_connection) {
                coordinator_connection->tls_connection->Close();
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

    void NodeTcpServer::SetDisconnectedHandler(DisconnectionHandler handler)
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