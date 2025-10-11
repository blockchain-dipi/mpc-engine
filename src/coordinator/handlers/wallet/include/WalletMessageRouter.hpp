// src/coordinator/handlers/wallet/include/WalletMessageRouter.hpp
#pragma once
#include "protocols/coordinator_wallet/include/WalletProtocolTypes.hpp"
#include <array>
#include <functional>
#include <memory>

namespace mpc_engine::coordinator::handlers::wallet
{
    using namespace protocol::coordinator_wallet;

    /**
     * @brief Wallet Message Router
     * 
     * - Node의 NodeMessageRouter와 동일한 패턴
     * - WalletMessageType → Handler 함수 매핑
     * - O(1) 라우팅
     */
    class WalletMessageRouter 
    {
    public:
        using HandlerFunction = std::function<std::unique_ptr<WalletBaseResponse>(const WalletBaseRequest*)>;

        static WalletMessageRouter& Instance() 
        {
            static WalletMessageRouter instance;
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
        WalletMessageRouter() = default;
        ~WalletMessageRouter() = default;

        WalletMessageRouter(const WalletMessageRouter&) = delete;
        WalletMessageRouter& operator=(const WalletMessageRouter&) = delete;

        // Handler 배열 (O(1) 접근)
        std::array<HandlerFunction, static_cast<size_t>(WalletMessageType::MAX_MESSAGE_TYPE)> handlers_{};
        
        bool initialized = false;
    };

} // namespace mpc_engine::coordinator::handlers::wallet