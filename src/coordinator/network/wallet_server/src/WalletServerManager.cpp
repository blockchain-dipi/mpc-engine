// src/coordinator/network/wallet_server/src/WalletServerManager.cpp
#include "coordinator/network/wallet_server/include/WalletServerManager.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>
#include <sstream>

namespace mpc_engine::coordinator::network
{
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

    WalletConnectionInfo WalletServerManager::GetConnectionInfo() const 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return conn_info_;
    }

} // namespace mpc_engine::coordinator::network