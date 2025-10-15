// src/node/network/src/NodeTcpServer.cpp
#include "node/network/include/NodeTcpServer.hpp"
#include "common/env/EnvManager.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/utils/firewall/KernelFirewall.hpp"
#include "common/utils/threading/ThreadUtils.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/resource/include/ReadOnlyResLoaderManager.hpp"
#include "common/utils/logger/Logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace mpc_engine::node::network
{
    using namespace mpc_engine::env;
    using namespace mpc_engine::kms;
    using namespace mpc_engine::resource;

    constexpr uint32_t THREAD_JOIN_TIMEOUT_MS = 5000;  // 5초

    NodeTcpServer::NodeTcpServer(const std::string& address, uint16_t port, size_t handler_threads)
    : bind_address(address), bind_port(port), num_handler_threads(handler_threads)
    {
        if (handler_threads == 0) {
            LOG_ERROR("NodeTcpServer", "handler_threads must be at least 1");
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
            LOG_WARN("NodeTcpServer", "Failed to initialize TLS context");
            return false;
        }

        auto& kms = KMSManager::Instance();
        try {
            // 1. CA 인증서 로드 (클라이언트 인증서 검증용) ← 추가 필요
            std::string tls_cert_path = EnvManager::Instance().GetString("TLS_CERT_PATH");
            std::string tls_ca = EnvManager::Instance().GetString("TLS_CERT_CA");

            std::string ca_pem = ReadOnlyResLoaderManager::Instance().ReadFile(tls_cert_path + tls_ca);
            if (ca_pem.empty()) {
                LOG_ERROR("NodeTcpServer", "Failed to load CA certificate from KMS");
                return false;
            }

            if (!tls_context->LoadCA(ca_pem)) {
                LOG_ERROR("NodeTcpServer", "Failed to load CA certificate to context");
                return false;
            }

            // 2. 서버 인증서 로드
            std::string certificate_pem = ReadOnlyResLoaderManager::Instance().ReadFile(tls_cert_path + certificate_path);
            std::string private_key_pem = kms.GetSecret(private_key_id);

            if (certificate_pem.empty() || private_key_pem.empty()) {
                LOG_ERROR("NodeTcpServer", "Failed to load certificate or private key");
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
            LOG_WARN("NodeTcpServer", "NodeTcpServer is already initialized");
            return true;
        }

        LOG_INFO("NodeTcpServer", "Initializing NodeTcpServer...");
        if (!InitializeTlsContext(certificate_path, private_key_id)) {
            LOG_ERROR("NodeTcpServer", "Failed to initialize TLS");
            return false;
        }

        // Create server socket
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET_VALUE) {
            LOG_ERROR("NodeTcpServer", "Failed to create server socket");
            return false;
        }

        SetSocketOptions(server_socket);

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(bind_port);
        
        if (inet_pton(AF_INET, bind_address.c_str(), &server_addr.sin_addr) <= 0) {
            LOG_ERRORF("NodeTcpServer", "Invalid bind address: %s", bind_address.c_str());
            utils::CloseSocket(server_socket);
            return false;
        }

        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            LOG_ERRORF("NodeTcpServer", "Failed to bind to %s:%d", bind_address.c_str(), bind_port);
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
        LOG_INFOF("NodeTcpServer", "NodeTcpServer initialized with %d handler threads", num_handler_threads);
        return true;
    }

    bool NodeTcpServer::Start() 
    {
        if (!is_initialized.load() || is_running.load()) {
            LOG_WARN("NodeTcpServer", "NodeTcpServer is already running or not initialized");
            return false;
        }

        if (listen(server_socket, 1) < 0) {
            LOG_ERROR("NodeTcpServer", "Failed to listen on socket");
            return false;
        }

        // 커널 방화벽 설정 (옵션)
        if (enable_kernel_firewall) {
            if (!security_config.trusted_coordinator_ip.empty()) {
                LOG_INFO("NodeTcpServer", "Configuring kernel-level firewall...");
                
                if (utils::KernelFirewall::ConfigureNodeFirewall(
                    bind_port, 
                    security_config.trusted_coordinator_ip
                )) {
                    LOG_INFO("NodeTcpServer", "✓ Kernel firewall active");
                    LOG_INFO("NodeTcpServer", "✓ SYN packets from untrusted IPs will be DROPped at kernel level");
                } else {
                    LOG_WARN("NodeTcpServer", "⚠ Failed to configure kernel firewall");
                    LOG_WARN("NodeTcpServer", "⚠ Falling back to application-level security only");
                }
            } else {
                LOG_ERROR("NodeTcpServer", "⚠ Kernel firewall enabled but no trusted IP set");
            }
        }

        is_running = true;
        accepting_connections = true;
        
        // Start Connection threads
        connection_thread = std::thread(&NodeTcpServer::ConnectionLoop, this);

        LOG_INFOF("NodeTcpServer", "NodeTcpServer started on %s:%d", bind_address.c_str(), bind_port);
        return true;
    }

    void NodeTcpServer::Stop() 
    {
        if (!is_running.load()) {
            LOG_WARN("NodeTcpServer", "NodeTcpServer is not running");
            return;
        }

        LOG_INFO("NodeTcpServer", "Stopping NodeTcpServer...");
        is_running = false;

        // 🔹 1단계: 서버 소켓 종료 (accept 차단)
        if (server_socket != INVALID_SOCKET_VALUE) {
            shutdown(server_socket, SHUT_RDWR);
            utils::CloseSocket(server_socket);
            server_socket = INVALID_SOCKET_VALUE;
        }

        // 🔹 2단계: 커널 방화벽 규칙 제거
        if (enable_kernel_firewall) {
            LOG_INFO("NodeTcpServer", "Removing kernel firewall rules...");
            utils::KernelFirewall::RemoveNodeFirewall(bind_port);
        }

        // 🔹 3단계: 기존 연결 강제 종료 (recv 차단)
        ForceCloseExistingConnection();

        // 🔹 4단계: ThreadPool Shutdown
        if (handler_pool) {
            LOG_INFO("NodeTcpServer", "Shutting down handlers...");
            handler_pool->Shutdown();
        }

        // 🔹 5단계: Send Queue Shutdown
        if (send_queue) {
            LOG_INFO("NodeTcpServer", "Shutting down send queue...");
            send_queue->Shutdown();
        }

        // 6단계: 스레드 안전 종료 (타임아웃 적용)
        LOG_INFOF("NodeTcpServer", "Waiting for threads to stop (timeout: %d ms)", THREAD_JOIN_TIMEOUT_MS);

        // Connection thread
        if (connection_thread.joinable()) {
            utils::JoinResult result = utils::JoinWithTimeout(connection_thread, THREAD_JOIN_TIMEOUT_MS);
            LOG_INFOF("NodeTcpServer", "  Connection thread: %s", utils::JoinResultToString(result));

            if (result == utils::JoinResult::TIMEOUT) {
                LOG_ERROR("NodeTcpServer", "  ⚠️  Connection thread did not stop in time!");
            }
        }

        // Receive thread
        if (receive_thread.joinable()) {
            utils::JoinResult result = utils::JoinWithTimeout(receive_thread, THREAD_JOIN_TIMEOUT_MS);
            LOG_INFOF("NodeTcpServer", "  Receive thread: %s", utils::JoinResultToString(result));
            
            if (result == utils::JoinResult::TIMEOUT) {
                LOG_ERROR("NodeTcpServer", "  ⚠️  Receive thread did not stop in time!");
            }
        }

        // Send thread
        if (send_thread.joinable()) {
            utils::JoinResult result = utils::JoinWithTimeout(send_thread, THREAD_JOIN_TIMEOUT_MS);
            LOG_INFOF("NodeTcpServer", "  Send thread: %s", utils::JoinResultToString(result));
            
            if (result == utils::JoinResult::TIMEOUT) {
                LOG_ERROR("NodeTcpServer", "  ⚠️  Send thread did not stop in time!");
            }
        }

        LOG_INFO("NodeTcpServer", "NodeTcpServer stopped");
    }

    bool NodeTcpServer::PrepareShutdown(uint32_t timeout_ms) 
    {
        LOG_INFO("NodeTcpServer", "========================================");
        LOG_INFO("NodeTcpServer", "  Preparing Graceful Shutdown");
        LOG_INFO("NodeTcpServer", "========================================");

        LOG_INFO("NodeTcpServer", "[1/3] Stopping new connections...");
        StopAcceptingConnections();

        LOG_INFO("NodeTcpServer", "[2/3] Waiting for pending requests...");
        auto start = std::chrono::steady_clock::now();
        bool completed = false;

        while (true) {
            uint32_t pending = GetPendingRequests();
            if (pending == 0) {
                LOG_INFO("NodeTcpServer", "  ✓ All requests completed");
                completed = true;
                break;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();

            if (elapsed > timeout_ms) {
                LOG_INFOF("NodeTcpServer", "  ⚠ Timeout: %d requests pending", pending);
                break;
            }

            LOG_INFOF("NodeTcpServer", "  Pending: %d (%dms)", pending, elapsed);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        LOG_INFO("NodeTcpServer", "[3/3] Additional cleanup...");
        // TODO: 나중에 여기에 DB 커밋, 로그 플러시 등 추가

        LOG_INFO("NodeTcpServer", "========================================");
        LOG_INFO("NodeTcpServer", completed ? "  ✓ Ready for shutdown" : "  ⚠ Forced shutdown");
        LOG_INFO("NodeTcpServer", "========================================");

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
        LOG_INFOF("NodeTcpServer", "ConnectionLoop Listening on %s:%d", bind_address.c_str(), bind_port);
        
        if (!security_config.trusted_coordinator_ip.empty()) {
            LOG_INFOF("NodeTcpServer", "[SECURITY] Trusted Coordinator: %s", security_config.trusted_coordinator_ip.c_str());
        }
        
        while (is_running.load()) {
            if (!accepting_connections.load()) {
                LOG_INFO("NodeTcpServer", "ConnectionLoop: Not accepting connections");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            socket_t client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);
            
            if (!is_running.load()) break;
            
            if (client_socket == INVALID_SOCKET_VALUE) {
                if (is_running.load()) {
                    LOG_ERROR("NodeTcpServer", "Accept failed");
                }
                continue;
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            uint16_t client_port = ntohs(client_addr.sin_port);

            // Security check
            if (!IsAuthorized(client_ip)) {
                LOG_ERRORF("NodeTcpServer", "[SECURITY] Rejected connection from %s:%d", client_ip, client_port);
                utils::CloseSocket(client_socket);
                continue;
            }

            // Close existing connection
            ForceCloseExistingConnection();

            LOG_INFOF("NodeTcpServer", "[SECURITY] Accepted connection from %s:%d", client_ip, client_port);
            HandleCoordinatorConnection(client_socket, client_ip, client_port);
        }
        
        LOG_INFO("NodeTcpServer", "Connection thread stopped");
    }

    void NodeTcpServer::HandleCoordinatorConnection(socket_t client_socket, const std::string& client_ip, uint16_t client_port) 
    {
        // TLS Connection 생성 및 핸드셰이크
        auto tls_connection = std::make_unique<TlsConnection>();

        TlsConnectionConfig tls_config;
        tls_config.handshake_timeout_ms = 10000;

        if (!tls_connection->AcceptServer(*tls_context, client_socket, tls_config)) {
            LOG_ERROR("NodeTcpServer", "TLS Accept failed");
            utils::CloseSocket(client_socket);
            return;
        }

        if (!tls_connection->DoHandshake()) {
            LOG_ERROR("NodeTcpServer", "TLS Handshake failed");
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

        // 스레드가 종료될 때까지 대기
        if (receive_thread.joinable()) {
            receive_thread.join();
        }

        if (send_thread.joinable()) {
            send_thread.join();
        }

        // 연결 종료 처리 (TLS Close 추가)
        std::cout << "[NodeTcpServer] Worker threads finished" << std::endl;
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
        LOG_DEBUG("NodeTcpServer", "Receive thread started");

        if (!message_handler) {
            LOG_ERROR("NodeTcpServer", "message_handler is null, cannot process messages");
            return;
        }

        while (is_running.load() && HasActiveConnection()) {
            NetworkMessage request;

            // socket 파라미터 제거 - ReceiveMessage 내부에서 TLS Connection 가져옴
            if (!ReceiveMessage(request)) {  // socket 파라미터는 더미값
                LOG_ERROR("NodeTcpServer", "Connection lost or receive failed");
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
        
            try {
                auto context = std::make_unique<HandlerContext>(
                    request, 
                    message_handler, 
                    send_queue.get()
                );
                handler_pool->SubmitOwned(ProcessMessage, std::move(context));

            } catch (const std::runtime_error& e) {
                LOG_ERRORF("NodeTcpServer", "Failed to submit task (pool stopped): %s", e.what());

                utils::QueueResult result = send_queue->TryPush(
                    CreateErrorResponse(request.header.message_type, "Server shutting down", -1),
                    std::chrono::milliseconds(100)
                );

                if (result != utils::QueueResult::SUCCESS) {
                    LOG_ERRORF("NodeTcpServer", "Failed to push error response: %s", utils::QueueResultToString(result));
                }

                break;

            } catch (const std::exception& e) {
                LOG_ERRORF("NodeTcpServer", "Failed to submit task: %s", e.what());

                utils::QueueResult result = send_queue->TryPush(
                    CreateErrorResponse(request.header.message_type, "Server busy", -1),
                    std::chrono::milliseconds(100)
                );

                if (result != utils::QueueResult::SUCCESS) {
                    LOG_ERRORF("NodeTcpServer", "Failed to push error response: %s", utils::QueueResultToString(result));
                }

                handler_errors++;
            }
        }

        LOG_DEBUG("NodeTcpServer", "Receive thread stopped");
    }

    void NodeTcpServer::SendLoop()
    {
        LOG_DEBUG("NodeTcpServer", "Send thread started");

        while (is_running.load() && HasActiveConnection()) {
            NetworkMessage outMessage;
            utils::QueueResult result = send_queue->Pop(outMessage);
            if (result != utils::QueueResult::SUCCESS) {
                LOG_ERRORF("NodeTcpServer", "Failed to pop message from send queue: %s", utils::QueueResultToString(result));
                continue;
            }
        
            // socket 파라미터 제거 - SendMessage 내부에서 TLS Connection 가져옴
            if (!SendMessage(outMessage)) {  // socket 파라미터는 더미값
                LOG_ERROR("NodeTcpServer", "Connection lost or send failed");
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

        LOG_DEBUG("NodeTcpServer", "Send thread stopped");
    }

    /**
    * @brief 메시지 처리 핸들러 (ThreadPool Worker에서 호출)
    * 
    * @param context ThreadPool Task가 소유권을 가진 포인터
    * 
    * @warning 
    * - context를 unique_ptr로 감싸지 말 것! (Double Free 발생)
    * - Task 소멸자에서 자동으로 delete 됨 (owned=true)
    * - 이 함수에서는 raw pointer로만 사용
    * 
    * @note
    * - 예외 발생 시에도 Task 소멸자가 정리 담당
    * - 함수 종료 시 명시적 delete 불필요
    */
    void NodeTcpServer::ProcessMessage(HandlerContext* context)
    {
        assert(context != nullptr && "HandlerContext must not be null");
        assert(context->send_queue != nullptr && "send_queue must not be null");

        uint64_t request_id = context->request.header.request_id;

        try {
            // 1. 요청 검증
            ValidationResult validation = context->request.Validate();
            if (validation != ValidationResult::OK) {
                LOG_ERRORF("NodeTcpServer", "Invalid request in handler: %s", ValidationResultToString(validation));

                utils::QueueResult result = context->send_queue->TryPush(
                    CreateErrorResponse(
                        context->request.header.message_type, 
                        std::string("Invalid request: ") + ValidationResultToString(validation),
                        request_id
                    ),
                    std::chrono::milliseconds(100)
                );

                if (result != utils::QueueResult::SUCCESS) {
                    LOG_ERRORF("NodeTcpServer", "Failed to push response: %s", utils::QueueResultToString(result));
                }
                return;
            }

            // 2. 핸들러 존재 확인
            if (!context->handler) {
                LOG_ERROR("NodeTcpServer", "Handler is null");

                utils::QueueResult result = context->send_queue->TryPush(
                    CreateErrorResponse(
                        context->request.header.message_type,
                        "Handler not configured",
                        request_id
                    ), 
                    std::chrono::milliseconds(100)
                );

                if (result != utils::QueueResult::SUCCESS) {
                    LOG_ERRORF("NodeTcpServer", "Failed to push response: %s", utils::QueueResultToString(result));
                }
                return;
            }

            // 3. 핸들러 호출
            NetworkMessage response = context->handler(context->request);

            // 4. ✅ Request ID 복사 (중요!)
            response.header.request_id = request_id;

            // 5. 응답 전송
            utils::QueueResult result = context->send_queue->TryPush(
                std::move(response), 
                std::chrono::milliseconds(5000)
            );

            if (result != utils::QueueResult::SUCCESS) {
                LOG_ERRORF("NodeTcpServer", "Failed to push response: %s", utils::QueueResultToString(result));
            }

        } catch (const std::exception& e) {
            LOG_ERRORF("NodeTcpServer", "Exception in ProcessMessage: %s", e.what());

            // 예외 발생 시에도 에러 응답 시도
            try {
                context->send_queue->TryPush(
                    CreateErrorResponse(
                        context->request.header.message_type,
                        "Internal error",
                        request_id
                    ),
                    std::chrono::milliseconds(100)
                );
            } catch (...) {
                LOG_ERROR("NodeTcpServer", "Failed to send error response");
            }
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
            LOG_ERROR("NodeTcpServer", "Failed to get TLS connection");
            return false;
        }

        // 기존 utils::SendExact 로직을 TLS로 교체
        TlsError error = tls_conn->WriteExact(&outMessage.header, sizeof(MessageHeader));
        if (error != TlsError::NONE) {
            LOG_ERRORF("NodeTcpServer", "Failed to send message header: %s", TlsErrorToString(error));
            return false;
        }

        if (outMessage.header.body_length > 0) {
            error = tls_conn->WriteExact(outMessage.body.data(), outMessage.body.size());
            if (error != TlsError::NONE) {
                LOG_ERRORF("NodeTcpServer", "Failed to send message body: %s", TlsErrorToString(error));
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
            LOG_ERROR("NodeTcpServer", "Failed to get TLS connection");
            return false;
        }

        // 기존 utils::ReceiveExact 로직을 TLS로 교체
        TlsError error = tls_conn->ReadExact(&outMessage.header, sizeof(MessageHeader));
        if (error != TlsError::NONE) {
            if (error == TlsError::CONNECTION_CLOSED) {
                LOG_ERROR("NodeTcpServer", "Connection closed gracefully");
            }
            return false;
        }

        // 기존 검증 로직 그대로 유지
        ValidationResult validation = outMessage.header.ValidateBasic();
        if (validation != ValidationResult::OK) {
            LOG_ERRORF("NodeTcpServer", "Header validation failed: %s", ValidationResultToString(validation));
            LOG_ERRORF("NodeTcpServer", "   Magic: 0x%x", outMessage.header.magic);
            LOG_ERRORF("NodeTcpServer", "   Version: %d", outMessage.header.version);
            LOG_ERRORF("NodeTcpServer", "   Body length: %d", outMessage.header.body_length);
            return false;
        }

        if (outMessage.header.body_length > 0) {
            try {
                outMessage.body.resize(outMessage.header.body_length);
            } catch (const std::bad_alloc& e) {
                LOG_ERRORF("NodeTcpServer", "Memory allocation failed for body: %s", e.what());
                return false;
            }
        
            error = tls_conn->ReadExact(outMessage.body.data(), outMessage.header.body_length);
            if (error != TlsError::NONE) {
                LOG_ERRORF("NodeTcpServer", "Failed to receive message body: %s", TlsErrorToString(error));
                return false;
            }
        }

        validation = outMessage.Validate();
        if (validation != ValidationResult::OK) {
            LOG_ERRORF("NodeTcpServer", "Message validation failed: %s", ValidationResultToString(validation));
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
            LOG_INFO("NodeTcpServer", "Closing existing connection");
            
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
        LOG_INFOF("NodeTcpServer", "Trusted Coordinator set to: %s", ip.c_str());
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
    NetworkMessage NodeTcpServer::CreateErrorResponse(uint16_t original_message_type, const std::string& error_message, uint64_t request_id)
    {
        std::string payload = "success=false|error=" + error_message;
        NetworkMessage error_msg(original_message_type, payload);
        error_msg.header.request_id = request_id;
        return error_msg;
    }
}