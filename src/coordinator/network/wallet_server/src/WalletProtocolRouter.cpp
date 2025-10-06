// src/coordinator/network/wallet_server/src/WalletProtocolRouter.cpp
#include "coordinator/network/wallet_server/include/WalletProtocolRouter.hpp"
#include "coordinator/handlers/wallet/include/WalletSigningProtocol.hpp"
#include <iostream>

namespace mpc_engine::coordinator::network
{
    using namespace protocol::coordinator_wallet;

    bool WalletProtocolRouter::Initialize() 
    {
        if (initialized) {
            return true;
        }

        std::cout << "[WalletProtocolRouter] Initializing..." << std::endl;

        // Handler 등록
        handlers_[static_cast<size_t>(WalletMessageType::SIGNING_REQUEST)] = handlers::wallet::HandleWalletSigningRequest;

        // STATUS_CHECK는 나중에 추가 가능
        // handlers_[static_cast<size_t>(WalletMessageType::STATUS_CHECK)] = ...

        initialized = true;
        std::cout << "[WalletProtocolRouter] Initialized successfully" << std::endl;
        
        return true;
    }

    std::unique_ptr<WalletBaseResponse> WalletProtocolRouter::ProcessMessage(
        WalletMessageType type,
        const WalletBaseRequest* request
    ) 
    {
        if (!initialized) {
            std::cerr << "[WalletProtocolRouter] Not initialized" << std::endl;
            return nullptr;
        }

        if (!request) {
            std::cerr << "[WalletProtocolRouter] Invalid request pointer" << std::endl;
            return nullptr;
        }

        size_t index = static_cast<size_t>(type);

        // 범위 체크
        if (index >= static_cast<size_t>(WalletMessageType::MAX_MESSAGE_TYPE)) {
            std::cerr << "[WalletProtocolRouter] Invalid message type: " 
                      << static_cast<uint32_t>(type) << std::endl;
            return nullptr;
        }

        // 핸들러 존재 체크
        if (!handlers_[index]) {
            std::cerr << "[WalletProtocolRouter] No handler for message type: " 
                      << ToString(type) << std::endl;
            return nullptr;
        }

        std::cout << "[WalletProtocolRouter] Processing message type: " 
                  << ToString(type) << std::endl;

        // O(1) 핸들러 호출
        return handlers_[index](request);
    }

} // namespace mpc_engine::coordinator::network