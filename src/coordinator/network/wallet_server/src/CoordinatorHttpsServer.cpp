// src/coordinator/network/wallet_server/src/CoordinatorHttpsServer.cpp
#include "coordinator/network/wallet_server/include/CoordinatorHttpsServer.hpp"
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/resource/include/ReadOnlyResLoaderManager.hpp"
#include "common/config/EnvManager.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <chrono>

namespace mpc_engine::coordinator::network::wallet_server
{
    using namespace mpc_engine::kms;
    using namespace mpc_engine::resource;

    CoordinatorHttpsServer::CoordinatorHttpsServer(const HttpsServerConfig& cfg)
        : config(cfg)
    {
        if (config.handler_threads == 0) {
            throw std::invalid_argument("handler_threads must be at least 1");
        }

        if (config.max_connections == 0) {
            throw std::invalid_argument("max_connections must be at least 1");
        }
    }

    CoordinatorHttpsServer::~CoordinatorHttpsServer() 
    {
        Stop();
    }

    bool CoordinatorHttpsServer::Initialize() 
    {
        if (initialized.load()) {
            std::cout << "[CoordinatorHttpsServer] Already initialized" << std::endl;
            return true;
        }

        std::cout << "[CoordinatorHttpsServer] Initializing..." << std::endl;

        // Step 1: WalletMessageRouter 초기화
        if (!handlers::wallet::WalletMessageRouter::Instance().Initialize()) {
            std::cerr << "[CoordinatorHttpsServer] Failed to initialize WalletMessageRouter" << std::endl;
            return false;
        }

        // Step 2: TLS Context 초기화
        if (!InitializeTlsContext()) {
            std::cerr << "[CoordinatorHttpsServer] Failed to initialize TLS context" << std::endl;
            return false;
        }

        // Step 3: Handler Pool 생성
        try {
            handler_pool = std::make_unique<ThreadPool<WalletHandlerContext>>(config.handler_threads);
            std::cout << "[CoordinatorHttpsServer] Handler pool created with " 
                      << config.handler_threads << " threads" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[CoordinatorHttpsServer] Failed to create handler pool: " << e.what() << std::endl;
            return false;
        }

        // Step 4: Listen Socket 생성
        if (!CreateListenSocket()) {
            std::cerr << "[CoordinatorHttpsServer] Failed to create listen socket" << std::endl;
            return false;
        }

        initialized.store(true);
        std::cout << "[CoordinatorHttpsServer] Initialized successfully" << std::endl;
        return true;
    }

    bool CoordinatorHttpsServer::Start() 
    {
        if (!initialized.load()) {
            std::cerr << "[CoordinatorHttpsServer] Not initialized" << std::endl;
            return false;
        }

        if (running.exchange(true)) {
            std::cout << "[CoordinatorHttpsServer] Already running" << std::endl;
            return true;
        }

        std::cout << "[CoordinatorHttpsServer] Starting..." << std::endl;

        // Accept Thread 시작
        accept_thread = std::thread(&CoordinatorHttpsServer::AcceptLoop, this);

        // Cleanup Thread 시작
        cleanup_thread = std::thread(&CoordinatorHttpsServer::CleanupLoop, this);

        std::cout << "[CoordinatorHttpsServer] Started successfully on " 
                  << config.bind_address << ":" << config.bind_port << std::endl;
        return true;
    }

    void CoordinatorHttpsServer::Stop() 
    {
        if (!running.exchange(false)) {
            return;
        }

        std::cout << "[CoordinatorHttpsServer] Stopping..." << std::endl;

        // Listen Socket 종료
        if (listen_socket != INVALID_SOCKET_VALUE) {
            ::close(listen_socket);
            listen_socket = INVALID_SOCKET_VALUE;
        }

        // Accept Thread 종료
        if (accept_thread.joinable()) {
            accept_thread.join();
        }

        // Cleanup Thread 종료
        if (cleanup_thread.joinable()) {
            cleanup_thread.join();
        }

        // 모든 연결 종료
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            for (auto& conn : connections) {
                if (conn) {
                    conn->Stop();
                }
            }
            connections.clear();
        }

        // Handler Pool 종료
        handler_pool.reset();

        std::cout << "[CoordinatorHttpsServer] Stopped" << std::endl;
    }

    size_t CoordinatorHttpsServer::GetActiveConnectionCount() const 
    {
        std::lock_guard<std::mutex> lock(connections_mutex);
        return connections.size();
    }

    void CoordinatorHttpsServer::AcceptLoop() 
    {
        std::cout << "[CoordinatorHttpsServer] Accept thread started" << std::endl;

        while (running.load()) 
        {
            // Accept 대기 (Blocking)
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            socket_t client_socket = ::accept(listen_socket, 
                                             (struct sockaddr*)&client_addr, 
                                             &client_len);

            if (client_socket == INVALID_SOCKET_VALUE) {
                if (running.load()) {
                    std::cerr << "[CoordinatorHttpsServer] Accept failed" << std::endl;
                }
                break;
            }

            // 클라이언트 정보
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            uint16_t client_port = ntohs(client_addr.sin_port);

            std::cout << "[CoordinatorHttpsServer] New connection from " 
                      << client_ip << ":" << client_port << std::endl;

            // 최대 연결 수 체크
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                if (connections.size() >= config.max_connections) {
                    std::cerr << "[CoordinatorHttpsServer] Max connections reached, rejecting" << std::endl;
                    ::close(client_socket);
                    continue;
                }
            }

            // TLS 핸드셰이크
            TlsConnection tls_conn;
            if (!tls_conn.AcceptServer(*tls_context, client_socket)) {
                std::cerr << "[CoordinatorHttpsServer] TLS handshake failed" << std::endl;
                ::close(client_socket);
                continue;
            }

            if (!tls_conn.DoHandshake()) {
                std::cerr << "[CoordinatorHttpsServer] TLS handshake failed" << std::endl;
                ::close(client_socket);
                continue;
            }

            std::cout << "[CoordinatorHttpsServer] TLS handshake completed" << std::endl;

            // WalletConnection 생성
            try {
                auto connection = std::make_unique<WalletConnection>(
                    std::move(tls_conn), 
                    handler_pool.get()
                );

                connection->Start();

                std::lock_guard<std::mutex> lock(connections_mutex);
                connections.push_back(std::move(connection));

                std::cout << "[CoordinatorHttpsServer] Connection established (total: " 
                          << connections.size() << ")" << std::endl;

            } catch (const std::exception& e) {
                std::cerr << "[CoordinatorHttpsServer] Failed to create connection: " 
                          << e.what() << std::endl;
                ::close(client_socket);
            }
        }

        std::cout << "[CoordinatorHttpsServer] Accept thread stopped" << std::endl;
    }

    void CoordinatorHttpsServer::CleanupLoop() 
    {
        std::cout << "[CoordinatorHttpsServer] Cleanup thread started" << std::endl;

        while (running.load()) 
        {
            std::this_thread::sleep_for(std::chrono::seconds(30));

            if (!running.load()) {
                break;
            }

            RemoveInactiveConnections();
        }

        std::cout << "[CoordinatorHttpsServer] Cleanup thread stopped" << std::endl;
    }

    void CoordinatorHttpsServer::RemoveInactiveConnections() 
    {
        std::lock_guard<std::mutex> lock(connections_mutex);

        auto it = connections.begin();
        while (it != connections.end()) 
        {
            bool should_remove = false;

            if (!(*it)->IsActive()) {
                should_remove = true;
                std::cout << "[CoordinatorHttpsServer] Removing inactive connection" << std::endl;
            }
            else if ((*it)->GetIdleTime() > config.idle_timeout_ms) {
                should_remove = true;
                std::cout << "[CoordinatorHttpsServer] Removing idle connection (timeout)" << std::endl;
                (*it)->Stop();
            }

            if (should_remove) {
                it = connections.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool CoordinatorHttpsServer::InitializeTlsContext() 
    {
        std::cout << "[CoordinatorHttpsServer] Initializing TLS context..." << std::endl;

        tls_context = std::make_unique<TlsContext>();

        TlsConfig tls_config = TlsConfig::CreateSecureServerConfig();
        if (!tls_context->Initialize(tls_config)) {
            std::cerr << "[CoordinatorHttpsServer] Failed to initialize TLS context" << std::endl;
            return false;
        }

        auto& kms = KMSManager::Instance();
        auto& res_loader = ReadOnlyResLoaderManager::Instance();

        try {
            // 1. CA 인증서 로드 (클라이언트 인증서 검증용)
            std::string tls_cert_path = env::EnvManager::Instance().GetString("TLS_CERT_PATH");
            std::string tls_ca = env::EnvManager::Instance().GetString("TLS_CERT_CA");

            std::string ca_pem = res_loader.ReadFile(tls_cert_path + tls_ca);
            if (ca_pem.empty()) {
                std::cerr << "[CoordinatorHttpsServer] Failed to load CA certificate" << std::endl;
                return false;
            }

            if (!tls_context->LoadCA(ca_pem)) {
                std::cerr << "[CoordinatorHttpsServer] Failed to load CA certificate to context" << std::endl;
                return false;
            }

            // 2. 서버 인증서 로드
            std::string certificate_pem = res_loader.ReadFile(tls_cert_path + config.tls_cert_path);
            std::string private_key_pem = kms.GetSecret(config.tls_key_id);

            if (certificate_pem.empty() || private_key_pem.empty()) {
                std::cerr << "[CoordinatorHttpsServer] Failed to load certificate or private key" << std::endl;
                return false;
            }

            CertificateData cert_data;
            cert_data.certificate_pem = certificate_pem;
            cert_data.private_key_pem = private_key_pem;

            if (!tls_context->LoadCertificate(cert_data)) {
                std::cerr << "[CoordinatorHttpsServer] Failed to load certificate into TLS context" << std::endl;
                return false;
            }

            std::cout << "[CoordinatorHttpsServer] TLS context initialized successfully" << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "[CoordinatorHttpsServer] TLS initialization exception: " 
                      << e.what() << std::endl;
            return false;
        }
    }

    bool CoordinatorHttpsServer::CreateListenSocket() 
    {
        std::cout << "[CoordinatorHttpsServer] Creating listen socket..." << std::endl;
    
        // Socket 생성
        listen_socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket == INVALID_SOCKET_VALUE) {
            std::cerr << "[CoordinatorHttpsServer] Failed to create socket: " 
                      << strerror(errno) << " (errno: " << errno << ")" << std::endl;
            return false;
        }
    
        std::cout << "[CoordinatorHttpsServer] Socket created: " << listen_socket << std::endl;
    
        // SO_REUSEADDR 설정
        int opt = 1;
        if (::setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "[CoordinatorHttpsServer] Failed to set SO_REUSEADDR: " 
                      << strerror(errno) << " (errno: " << errno << ")" << std::endl;
            ::close(listen_socket);
            listen_socket = INVALID_SOCKET_VALUE;
            return false;
        }
    
        std::cout << "[CoordinatorHttpsServer] SO_REUSEADDR set" << std::endl;
    
        // SO_REUSEPORT 설정
        opt = 1;
        if (::setsockopt(listen_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            std::cerr << "[CoordinatorHttpsServer] Warning: Failed to set SO_REUSEPORT: " 
                      << strerror(errno) << " (errno: " << errno << ")" << std::endl;
        } else {
            std::cout << "[CoordinatorHttpsServer] SO_REUSEPORT set" << std::endl;
        }
    
        // SO_LINGER 설정 (연결 종료 시 즉시 포트 해제)
        struct linger sl;
        sl.l_onoff = 1;   // linger 활성화
        sl.l_linger = 0;  // 즉시 종료
        if (::setsockopt(listen_socket, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl)) < 0) {
            std::cerr << "[CoordinatorHttpsServer] Warning: Failed to set SO_LINGER: " 
                      << strerror(errno) << std::endl;
        } else {
            std::cout << "[CoordinatorHttpsServer] SO_LINGER set (immediate close)" << std::endl;
        }
    
        // Bind
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config.bind_port);
    
        std::cout << "[CoordinatorHttpsServer] Preparing to bind to " 
                  << config.bind_address << ":" << config.bind_port << std::endl;
    
        if (config.bind_address == "0.0.0.0") {
            server_addr.sin_addr.s_addr = INADDR_ANY;
            std::cout << "[CoordinatorHttpsServer] Using INADDR_ANY" << std::endl;
        } else {
            if (inet_pton(AF_INET, config.bind_address.c_str(), &server_addr.sin_addr) <= 0) {
                std::cerr << "[CoordinatorHttpsServer] Invalid bind address: " 
                          << config.bind_address << std::endl;
                ::close(listen_socket);
                listen_socket = INVALID_SOCKET_VALUE;
                return false;
            }
            std::cout << "[CoordinatorHttpsServer] Address converted successfully" << std::endl;
        }
    
        if (::bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            int bind_errno = errno;
            std::cerr << "[CoordinatorHttpsServer] Failed to bind to " 
                      << config.bind_address << ":" << config.bind_port 
                      << " - Error: " << strerror(bind_errno) 
                      << " (errno: " << bind_errno << ")" << std::endl;
            
            if (bind_errno == EADDRINUSE) {
                std::cerr << "[CoordinatorHttpsServer] Port " << config.bind_port 
                          << " is already in use" << std::endl;
            } else if (bind_errno == EACCES) {
                std::cerr << "[CoordinatorHttpsServer] Permission denied for port " 
                          << config.bind_port << std::endl;
            } else if (bind_errno == EADDRNOTAVAIL) {
                std::cerr << "[CoordinatorHttpsServer] Address not available" << std::endl;
            }
            
            ::close(listen_socket);
            listen_socket = INVALID_SOCKET_VALUE;
            return false;
        }
    
        std::cout << "[CoordinatorHttpsServer] Bind successful" << std::endl;
    
        // Listen
        if (::listen(listen_socket, 128) < 0) {
            std::cerr << "[CoordinatorHttpsServer] Failed to listen: " 
                      << strerror(errno) << " (errno: " << errno << ")" << std::endl;
            ::close(listen_socket);
            listen_socket = INVALID_SOCKET_VALUE;
            return false;
        }
    
        std::cout << "[CoordinatorHttpsServer] Listen socket created successfully on " 
                  << config.bind_address << ":" << config.bind_port << std::endl;
        return true;
    }

} // namespace mpc_engine::coordinator::network::wallet_server