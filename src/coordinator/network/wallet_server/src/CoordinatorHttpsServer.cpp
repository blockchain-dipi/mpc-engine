// src/coordinator/network/wallet_server/src/CoordinatorHttpsServer.cpp
#include "coordinator/network/wallet_server/include/CoordinatorHttpsServer.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/resource/include/ReadOnlyResLoaderManager.hpp"
#include "common/env/EnvManager.hpp"
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include "common/utils/logger/Logger.hpp"
#include <thread>

namespace mpc_engine::coordinator::network::wallet_server
{
    using namespace mpc_engine::kms;
    using namespace mpc_engine::resource;
    using namespace mpc_engine::coordinator::handlers::wallet;

    CoordinatorHttpsServer::CoordinatorHttpsServer(const HttpsServerConfig& cfg)
        : config_(cfg)
        , io_context_()
    {
        if (config_.handler_threads == 0) {
            LOG_ERROR("CoordinatorHttpsServer", "handler_threads must be at least 1");
            throw std::invalid_argument("handler_threads must be at least 1");
        }

        if (config_.max_connections == 0) {
            LOG_ERROR("CoordinatorHttpsServer", "max_connections must be at least 1");
            throw std::invalid_argument("max_connections must be at least 1");
        }
    }

    CoordinatorHttpsServer::~CoordinatorHttpsServer() 
    {
        Stop();
    }

    bool CoordinatorHttpsServer::Initialize() 
    {
        if (initialized_.load()) {
            LOG_INFO("CoordinatorHttpsServer", "Already initialized");
            return true;
        }

        LOG_INFO("CoordinatorHttpsServer", "Initializing...");

        // Step 1: WalletMessageRouter 초기화 체크
        if (!InitializeRouter()) {
            LOG_ERROR("CoordinatorHttpsServer", "Failed to initialize WalletMessageRouter");
            return false;
        }

        // Step 2: TLS Context 초기화
        if (!InitializeTlsContext()) {
            LOG_ERROR("CoordinatorHttpsServer", "Failed to initialize TLS context");
            return false;
        }

        // Step 3: ThreadPool 생성
        if (!InitializeThreadPool()) {
            LOG_ERROR("CoordinatorHttpsServer", "Failed to initialize ThreadPool");
            return false;
        }

        // Step 4: Acceptor 생성
        try {
            tcp::endpoint endpoint(
                asio::ip::make_address(config_.bind_address),
                config_.bind_port
            );

            acceptor_ = std::make_unique<tcp::acceptor>(io_context_);
            acceptor_->open(endpoint.protocol());
            acceptor_->set_option(asio::socket_base::reuse_address(true));
            acceptor_->bind(endpoint);
            acceptor_->listen(asio::socket_base::max_listen_connections);

            LOG_INFOF("CoordinatorHttpsServer", "Listening on %s:%d", config_.bind_address.c_str(), config_.bind_port);
        } catch (const std::exception& e) {
            LOG_ERRORF("CoordinatorHttpsServer", "Failed to create acceptor: %s", e.what());
            return false;
        }

        initialized_ = true;
        LOG_INFO("CoordinatorHttpsServer", "Initialization complete");
        return true;
    }

    bool CoordinatorHttpsServer::Start() 
    {
        if (!initialized_.load()) {
            LOG_ERROR("CoordinatorHttpsServer", "Not initialized");
            return false;
        }

        if (running_.exchange(true)) {
            LOG_INFO("CoordinatorHttpsServer", "Already running");
            return true;
        }

        LOG_INFO("CoordinatorHttpsServer", "Starting...");
        // Accept Loop 시작
        DoAccept();

        // io_context 스레드 수 결정
        size_t num_io_threads = config_.io_threads;
        if (num_io_threads == 0) {
            num_io_threads = std::thread::hardware_concurrency();
            if (num_io_threads == 0) {
                num_io_threads = 4;  // fallback
            }
        }

        LOG_INFOF("CoordinatorHttpsServer", "Starting %zu I/O threads", num_io_threads);

        // io_context를 여러 스레드에서 실행
        io_threads_.reserve(num_io_threads);
        for (size_t i = 0; i < num_io_threads; ++i) {
            io_threads_.emplace_back([this, i]() {
                LOG_INFOF("CoordinatorHttpsServer", "I/O thread %zu started", i);
                try {
                    io_context_.run();
                } catch (const std::exception& e) {
                    LOG_ERRORF("CoordinatorHttpsServer", "I/O thread %zu exception: %s", i, e.what());
                }
                LOG_INFOF("CoordinatorHttpsServer", "I/O thread %zu stopped", i);
            });
        }

        LOG_INFO("CoordinatorHttpsServer", "Started successfully");
        return true;
    }

    void CoordinatorHttpsServer::Stop() 
    {
        if (!running_.exchange(false)) {
            LOG_INFO("CoordinatorHttpsServer", "Already stopped");
            return;
        }

        LOG_INFO("CoordinatorHttpsServer", "Stopping...");

        // Acceptor 중지
        if (acceptor_) {
            boost::system::error_code ec;
            
            if (acceptor_->is_open()) {
                // NOLINTNEXTLINE(bugprone-unused-return-value)
                acceptor_->close(ec);
                if (ec) {
                    LOG_ERRORF("CoordinatorHttpsServer", "Acceptor close error: %s", ec.message().c_str());
                }
            }
        }
    
        // io_context 중지
        io_context_.stop();
    
        // I/O 스레드 종료 대기
        for (auto& thread : io_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        io_threads_.clear();
    
        // ThreadPool 종료
        if (handler_pool_) {
            handler_pool_->Shutdown();
        }
    
        LOG_INFO("CoordinatorHttpsServer", "Stopped");
    }

    void CoordinatorHttpsServer::DoAccept() 
    {
        if (!running_.load()) {
            LOG_WARN("CoordinatorHttpsServer", "Server not running, skipping accept");
            return;
        }

        acceptor_->async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    LOG_INFOF("CoordinatorHttpsServer", "New connection accepted from %s:%d", 
                        socket.remote_endpoint().address().to_string().c_str(), socket.remote_endpoint().port());

                    // 새 HttpsSession 생성 및 시작
                    auto session = std::make_shared<HttpsSession>(
                        std::move(socket),
                        *ssl_context_,
                        *handler_pool_,
                        WalletMessageRouter::Instance(),
                        config_.max_requests_per_connection,
                        std::chrono::seconds(config_.keep_alive_timeout_sec)
                    );
                    session->Start();
                } else {
                    LOG_ERRORF("CoordinatorHttpsServer", "Accept error: %s", ec.message().c_str());
                }

                // 다음 연결 수락 대기
                if (running_.load()) {
                    DoAccept();
                }
            }
        );
    }

    bool CoordinatorHttpsServer::InitializeTlsContext() 
    {
        LOG_INFO("CoordinatorHttpsServer", "Initializing TLS context...");

        try {
            ssl_context_ = std::make_unique<ssl::context>(ssl::context::tlsv12_server);

            // TLS 옵션 설정 (기존 TlsConfig::CreateSecureServerConfig()와 유사)
            ssl_context_->set_options(
                ssl::context::default_workarounds |
                ssl::context::no_sslv2 |
                ssl::context::no_sslv3 |
                ssl::context::no_tlsv1 |
                ssl::context::no_tlsv1_1 |
                ssl::context::single_dh_use
            );

            // SSL/TLS 검증 모드 설정
            ssl_context_->set_verify_mode(
                ssl::verify_peer | 
                ssl::verify_fail_if_no_peer_cert |
                ssl::verify_client_once
            );

            auto& kms = KMSManager::Instance();
            auto& res_loader = ReadOnlyResLoaderManager::Instance();

            // 환경 변수에서 경로 읽기
            std::string tls_cert_path = env::EnvManager::Instance().GetString("TLS_CERT_PATH");
            std::string tls_ca = env::EnvManager::Instance().GetString("TLS_CERT_CA");
            std::string coordinator_cert = env::EnvManager::Instance().GetString("TLS_CERT_COORDINATOR_WALLET");
            std::string coordinator_key_id = env::EnvManager::Instance().GetString("TLS_KMS_COORDINATOR_WALLET_KEY_ID");

            // 1. CA 인증서 로드 (클라이언트 인증서 검증용 - mTLS)
            LOG_INFOF("CoordinatorHttpsServer", "Loading CA certificate: %s", (tls_cert_path + tls_ca).c_str());

            std::string ca_pem = res_loader.ReadFile(tls_cert_path + tls_ca);
            if (ca_pem.empty()) {
                LOG_ERROR("CoordinatorHttpsServer", "Failed to load CA certificate");
                return false;
            }

            ssl_context_->load_verify_file(tls_cert_path + tls_ca);

            // 2. 서버 인증서 로드
            LOG_INFOF("CoordinatorHttpsServer", "Loading server certificate: %s", (tls_cert_path + coordinator_cert).c_str());

            std::string certificate_pem = res_loader.ReadFile(tls_cert_path + coordinator_cert);
            if (certificate_pem.empty()) {
                LOG_ERROR("CoordinatorHttpsServer", "Failed to load server certificate");
                return false;
            }

            // 인증서 체인 로드
            ssl_context_->use_certificate_chain_file(tls_cert_path + coordinator_cert);

            // 3. 개인키 로드 (KMS에서)
            LOG_INFOF("CoordinatorHttpsServer", "Loading private key from KMS: %s", coordinator_key_id.c_str());

            std::string private_key_pem = kms.GetSecret(coordinator_key_id);
            if (private_key_pem.empty()) {
                LOG_ERRORF("CoordinatorHttpsServer", "Failed to load private key from KMS: %s", coordinator_key_id.c_str());
                return false;
            }

            // 개인키를 메모리에서 로드
            boost::asio::const_buffer key_buffer(private_key_pem.data(), private_key_pem.size());
            ssl_context_->use_private_key(key_buffer, ssl::context::pem);

            // 4. 암호 스위트 설정 (강력한 암호만 허용)
            SSL_CTX_set_cipher_list(ssl_context_->native_handle(), 
                "ECDHE-ECDSA-AES256-GCM-SHA384:"
                "ECDHE-RSA-AES256-GCM-SHA384:"
                "ECDHE-ECDSA-AES128-GCM-SHA256:"
                "ECDHE-RSA-AES128-GCM-SHA256");

            LOG_INFO("CoordinatorHttpsServer", "TLS context initialized successfully with mTLS");
            return true;

        } catch (const std::exception& e) {
            LOG_ERRORF("CoordinatorHttpsServer", "TLS context initialization failed: %s", e.what());
            return false;
        }
    }

    bool CoordinatorHttpsServer::InitializeThreadPool() 
    {
        LOG_INFOF("CoordinatorHttpsServer", "Creating ThreadPool with %d threads", config_.handler_threads);

        try {
            handler_pool_ = std::make_unique<ThreadPool<WalletHandlerContext>>(
                config_.handler_threads
            );
            LOG_INFO("CoordinatorHttpsServer", "ThreadPool created successfully");
            return true;
        } catch (const std::exception& e) {
            LOG_ERRORF("CoordinatorHttpsServer", "Failed to create ThreadPool: %s", e.what());
            return false;
        }
    }

    bool CoordinatorHttpsServer::InitializeRouter() 
    {
        LOG_INFO("CoordinatorHttpsServer", "Initializing WalletMessageRouter...");

        if (!WalletMessageRouter::Instance().Initialize()) {
            LOG_ERROR("CoordinatorHttpsServer", "Failed to initialize WalletMessageRouter");
            return false;
        }

        LOG_INFO("CoordinatorHttpsServer", "WalletMessageRouter initialized");
        return true;
    }

} // namespace mpc_engine::coordinator::network::wallet_server