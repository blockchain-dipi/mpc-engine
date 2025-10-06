// src/coordinator/network/wallet_server/include/WalletProtocolRouter.hpp
#pragma once
#include "protocols/coordinator_wallet/include/WalletProtocolTypes.hpp"
#include <array>
#include <functional>
#include <memory>

namespace mpc_engine::coordinator::network
{
    using namespace protocol::coordinator_wallet;

    /**
     * @brief Wallet Protocol Router
     * 
     * - Node의 NodeProtocolRouter와 동일한 패턴
     * - WalletMessageType → Handler 함수 매핑
     * - O(1) 라우팅
     */
    class WalletProtocolRouter 
    {
    public:
        using HandlerFunction = std::function<std::unique_ptr<WalletBaseResponse>(const WalletBaseRequest*)>;

        static WalletProtocolRouter& Instance() 
        {
            static WalletProtocolRouter instance;
            return instance;
        }

        /**
         * @brief Router 초기화 (Handler 등록)
         */
        bool Initialize();

        /**
         * @brief 메시지 처리
         * 
         * @param type WalletMessageType
         * @param request 요청 객체
         * @return 응답 객체 (실패 시 nullptr)
         */
        std::unique_ptr<WalletBaseResponse> ProcessMessage(
            WalletMessageType type,
            const WalletBaseRequest* request
        );

    private:
        WalletProtocolRouter() = default;
        ~WalletProtocolRouter() = default;

        WalletProtocolRouter(const WalletProtocolRouter&) = delete;
        WalletProtocolRouter& operator=(const WalletProtocolRouter&) = delete;

        // Handler 배열 (O(1) 접근)
        std::array<HandlerFunction, 
                   static_cast<size_t>(WalletMessageType::MAX_MESSAGE_TYPE)> handlers_{};
        
        bool initialized = false;
    };

} // namespace mpc_engine::coordinator::network