// src/coordinator/handlers/wallet/src/WalletMessageRouter.cpp
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include "coordinator/handlers/wallet/include/WalletSigningHandler.hpp"
#include "common/utils/logger/Logger.hpp"

namespace mpc_engine::coordinator::handlers::wallet
{
    bool WalletMessageRouter::Initialize() 
    {
        if (initialized) {
            return true;
        }

        LOG_INFO("WalletMessageRouter", "Initializing...");

        // Handler 등록
        handlers_[static_cast<size_t>(mpc_engine::WalletMessageType::SIGNING_REQUEST)] = HandleWalletSigningRequest;

        // STATUS_CHECK는 나중에 추가 가능
        // handlers_[static_cast<size_t>(WalletMessageType::STATUS_CHECK)] = ...

        initialized = true;
        LOG_INFO("WalletMessageRouter", "Initialized successfully");
        
        return true;
    }

    std::unique_ptr<WalletCoordinatorMessage> WalletMessageRouter::ProcessMessage(const WalletCoordinatorMessage* request) 
    {
        LOG_DEBUGF("WalletMessageRouter", "Processing message type: %s", 
            WalletMessageTypeToString(static_cast<mpc_engine::WalletMessageType>(request->message_type())));

        if (!initialized) {
            LOG_ERROR("WalletMessageRouter", "Not initialized");
            return nullptr;
        }

        if (!request) {
            LOG_ERROR("WalletMessageRouter", "Invalid request pointer");
            return nullptr;
        }

        uint32_t message_type = request->message_type();
        
        // WalletMessageType으로 변환
        auto type = static_cast<mpc_engine::WalletMessageType>(message_type);
        size_t index = static_cast<size_t>(type);

        // 범위 체크
        if (index >= static_cast<size_t>(mpc_engine::WalletMessageType::MAX_MESSAGE_TYPE)) {
            LOG_ERRORF("WalletMessageRouter", "Invalid message type: %d", message_type);
            return nullptr;
        }

        // 핸들러 존재 체크
        if (!handlers_[index]) {
            LOG_ERRORF("WalletMessageRouter", "No handler for message type: %s", WalletMessageTypeToString(type));
            return nullptr;
        }

        LOG_DEBUGF("WalletMessageRouter", "Processing message type: %s", WalletMessageTypeToString(type));

        return handlers_[index](request);
    }

} // namespace mpc_engine::coordinator::handlers::wallet