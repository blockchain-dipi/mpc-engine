// src/coordinator/handlers/wallet/include/WalletMessageRouter.hpp
#pragma once
#include "proto/wallet_coordinator/generated/wallet_message.pb.h"
#include "types/WalletMessageType.hpp"
#include <array>
#include <functional>
#include <memory>

namespace mpc_engine::coordinator::handlers::wallet
{
    using namespace mpc_engine::proto::wallet_coordinator;

    class WalletMessageRouter 
    {
    public:
        using HandlerFunction = std::function<std::unique_ptr<WalletCoordinatorMessage>(const WalletCoordinatorMessage*)>;

        static WalletMessageRouter& Instance() 
        {
            static WalletMessageRouter instance;
            return instance;
        }

        bool Initialize();
        std::unique_ptr<WalletCoordinatorMessage> ProcessMessage(const WalletCoordinatorMessage* request);

    private:
        WalletMessageRouter() = default;
        ~WalletMessageRouter() = default;

        WalletMessageRouter(const WalletMessageRouter&) = delete;
        WalletMessageRouter& operator=(const WalletMessageRouter&) = delete;

        std::array<HandlerFunction, static_cast<size_t>(mpc_engine::WalletMessageType::MAX_MESSAGE_TYPE)> handlers_{};
        
        bool initialized = false;
    };

} // namespace mpc_engine::coordinator::handlers::wallet