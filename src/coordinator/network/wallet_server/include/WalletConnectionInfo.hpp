// src/coordinator/network/wallet_server/include/WalletConnectionInfo.hpp
#pragma once
#include <string>
#include <cstdint>

namespace mpc_engine::coordinator::network
{
    /**
     * @brief Wallet Server 연결 설정 (설정값만)
     */
    struct WalletConnectionInfo 
    {
        std::string wallet_url;       // "https://wallet.example.com/api/v1"
        std::string host;              // "wallet.example.com"
        uint16_t port = 443;
        std::string path_prefix;       // "/api/v1"
        std::string auth_token;        // "Bearer xyz..."
        uint32_t timeout_ms = 30000;
        uint32_t max_retries = 3;
        
        bool ParseUrl(const std::string& url);
    };

} // namespace