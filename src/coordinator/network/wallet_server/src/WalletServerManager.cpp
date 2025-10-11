// src/coordinator/network/wallet_server/src/WalletServerManager.cpp
#include "coordinator/network/wallet_server/include/WalletServerManager.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>
#include <sstream>

namespace mpc_engine::coordinator::network
{
    using namespace https;

    bool WalletServerManager::Initialize(
        const std::string& wallet_url,
        const std::string& auth_token,
        const TlsContext& tls_ctx
    ) 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (is_initialized_.load()) {
            std::cout << "[WalletServerManager] Already initialized" << std::endl;
            return true;
        }

        std::cout << "[WalletServerManager] Initializing..." << std::endl;

        // 1. URL 파싱
        conn_info_.wallet_url = wallet_url;
        conn_info_.auth_token = auth_token;

        if (!conn_info_.ParseUrl(wallet_url)) {
            std::cerr << "[WalletServerManager] Failed to parse URL: " << wallet_url << std::endl;
            return false;
        }

        // 2. Protocol Router 초기화
        if (!router_.Initialize()) {
            std::cerr << "[WalletServerManager] Failed to initialize router" << std::endl;
            return false;
        }

        // 3. Connection Pool 생성
        ConnectionPoolConfig pool_config;
        pool_config.max_connections = 10;
        pool_config.min_idle = 2;
        pool_config.max_idle_time_ms = 60000;
        pool_ = std::make_unique<HttpsConnectionPool>(pool_config);
        if (!pool_->Initialize(tls_ctx)) {
            std::cerr << "[WalletServerManager] Failed to initialize connection pool" << std::endl;
            return false;
        }

        is_initialized_ = true;

        std::cout << "[WalletServerManager] Initialized successfully" << std::endl;
        std::cout << "  URL: " << conn_info_.wallet_url << std::endl;
        std::cout << "  Host: " << conn_info_.host << ":" << conn_info_.port << std::endl;
        std::cout << "  Path Prefix: " << conn_info_.path_prefix << std::endl;

        return true;
    }

    // std::unique_ptr<WalletSigningResponse> WalletServerManager::SendSigningRequest(const WalletSigningRequest& request) 
    // {
    //     if (!is_initialized_.load()) {
    //         std::cerr << "[WalletServerManager] Not initialized" << std::endl;
    //         return nullptr;
    //     }

    //     std::cout << "[WalletServerManager] Sending signing request..." << std::endl;

    //     try 
    //     {
    //         // 1. Connection Pool에서 연결 획득
    //         HttpsClient* client = pool_->Acquire(conn_info_.host, conn_info_.port);
    //         if (!client) {
    //             std::cerr << "[WalletServerManager] Failed to acquire connection" << std::endl;
    //             return nullptr;
    //         }

    //         // 2. JSON 직렬화
    //         std::string json_body = request.ToJson();
    //         std::cout << "[WalletServerManager] Request JSON: " << json_body << std::endl;

    //         // 3. HTTPS POST 요청
    //         std::string path = conn_info_.path_prefix + "/sign";
    //         std::string request_id = "req_" + std::to_string(utils::GetCurrentTimeMs());

    //         char response_buffer[8192];
    //         HttpResponseReader::Response http_response;

    //         bool success = client->PostJson(
    //             path,
    //             conn_info_.auth_token,
    //             request_id,
    //             json_body,
    //             response_buffer,
    //             sizeof(response_buffer),
    //             http_response
    //         );

    //         // 4. 연결 반환
    //         pool_->Release(client);

    //         if (!success || !http_response.success) {
    //             std::cerr << "[WalletServerManager] HTTP request failed" << std::endl;
    //             std::cerr << "  Status: " << http_response.status_code << std::endl;
    //             return nullptr;
    //         }

    //         // 5. JSON 역직렬화
    //         std::string response_json(http_response.body, http_response.body_len);
    //         std::cout << "[WalletServerManager] Response JSON: " << response_json << std::endl;

    //         auto wallet_response = std::make_unique<WalletSigningResponse>();
    //         if (!wallet_response->FromJson(response_json)) {
    //             std::cerr << "[WalletServerManager] Failed to parse response JSON" << std::endl;
    //             return nullptr;
    //         }

    //         std::cout << "[WalletServerManager] Request completed successfully" << std::endl;
    //         std::cout << "  Success: " << wallet_response->success << std::endl;
    //         if (wallet_response->success) {
    //             std::cout << "  Signature: " << wallet_response->finalSignature << std::endl;
    //         }

    //         return wallet_response;
    //     }
    //     catch (const std::exception& e) 
    //     {
    //         std::cerr << "[WalletServerManager] Exception: " << e.what() << std::endl;
    //         return nullptr;
    //     }
    // }

    WalletConnectionInfo WalletServerManager::GetConnectionInfo() const 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return conn_info_;
    }

} // namespace mpc_engine::coordinator::network