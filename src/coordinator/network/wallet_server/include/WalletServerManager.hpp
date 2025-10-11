// src/coordinator/network/wallet_server/include/WalletServerManager.hpp
#pragma once
#include "coordinator/network/wallet_server/include/WalletConnectionInfo.hpp"
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include "common/https/include/HttpsConnectionPool.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include "protocols/coordinator_wallet/include/WalletProtocolTypes.hpp"
#include <memory>
#include <mutex>
#include <atomic>

namespace mpc_engine::coordinator::network
{
    using namespace mpc_engine::protocol::coordinator_wallet;    
    using namespace mpc_engine::coordinator::handlers::wallet;
    using namespace mpc_engine::network::tls;

    /**
     * @brief Wallet Server Manager (Singleton)
     * 
     * - HttpsConnectionPool 관리
     * - WalletMessageRouter 통합
     * - Coordinator가 사용하는 고수준 API
     */
    class WalletServerManager 
    {
    public:
        static WalletServerManager& Instance() 
        {
            static WalletServerManager instance;
            return instance;
        }

        /**
         * @brief 초기화
         * 
         * @param wallet_url Wallet Server URL
         * @param auth_token Authorization token
         * @param tls_ctx TLS Context (KMS에서 CA 로드 완료)
         * @return 성공 여부
         */
        bool Initialize(
            const std::string& wallet_url,
            const std::string& auth_token,
            const TlsContext& tls_ctx
        );

        // /**
        //  * @brief 서명 요청 전송
        //  * 
        //  * @param request WalletSigningRequest
        //  * @return WalletSigningResponse (실패 시 nullptr)
        //  */
        // std::unique_ptr<WalletSigningResponse> 
        // SendSigningRequest(const WalletSigningRequest& request);

        /**
         * @brief 연결 정보 조회
         */
        WalletConnectionInfo GetConnectionInfo() const;

        /**
         * @brief 초기화 여부
         */
        bool IsInitialized() const { return is_initialized_.load(); }

    private:
        WalletServerManager() = default;
        ~WalletServerManager() = default;

        WalletServerManager(const WalletServerManager&) = delete;
        WalletServerManager& operator=(const WalletServerManager&) = delete;

        // Connection Pool
        std::unique_ptr<https::HttpsConnectionPool> pool_;

        // Message Router
        WalletMessageRouter& router_ = WalletMessageRouter::Instance();

        // 연결 정보
        WalletConnectionInfo conn_info_;
        
        // 상태
        std::atomic<bool> is_initialized_{false};
        mutable std::mutex mutex_;
    };

} // namespace mpc_engine::coordinator::network