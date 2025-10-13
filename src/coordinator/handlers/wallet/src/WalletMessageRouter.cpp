// src/coordinator/handlers/wallet/src/WalletMessageRouter.cpp
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include "coordinator/handlers/wallet/include/WalletSigningHandler.hpp"
#include <iostream>

namespace mpc_engine::coordinator::handlers::wallet
{
    bool WalletMessageRouter::Initialize() 
    {
        if (initialized) {
            return true;
        }

        std::cout << "[WalletMessageRouter] Initializing..." << std::endl;

        // Handler 등록
        handlers_[static_cast<size_t>(mpc_engine::WalletMessageType::SIGNING_REQUEST)] = HandleWalletSigningRequest;

        // STATUS_CHECK는 나중에 추가 가능
        // handlers_[static_cast<size_t>(WalletMessageType::STATUS_CHECK)] = ...

        initialized = true;
        std::cout << "[WalletMessageRouter] Initialized successfully" << std::endl;
        
        return true;
    }

    std::unique_ptr<WalletCoordinatorMessage> WalletMessageRouter::ProcessMessage(const WalletCoordinatorMessage* request) 
    {
        if (!initialized) {
            std::cerr << "[WalletMessageRouter] Not initialized" << std::endl;
            return nullptr;
        }

        if (!request) {
            std::cerr << "[WalletMessageRouter] Invalid request pointer" << std::endl;
            return nullptr;
        }

        uint32_t message_type = request->message_type();
        
        // WalletMessageType으로 변환
        auto type = static_cast<mpc_engine::WalletMessageType>(message_type);
        size_t index = static_cast<size_t>(type);

        // 범위 체크
        if (index >= static_cast<size_t>(mpc_engine::WalletMessageType::MAX_MESSAGE_TYPE)) {
            std::cerr << "[WalletMessageRouter] Invalid message type: " << message_type << std::endl;
            return nullptr;
        }

        // 핸들러 존재 체크
        if (!handlers_[index]) {
            std::cerr << "[WalletMessageRouter] No handler for message type: " 
                      << WalletMessageTypeToString(type) << std::endl;
            return nullptr;
        }

        std::cout << "[WalletMessageRouter] Processing message type: " 
                  << WalletMessageTypeToString(type) << std::endl;

        return handlers_[index](request);
    }

} // namespace mpc_engine::coordinator::handlers::wallet