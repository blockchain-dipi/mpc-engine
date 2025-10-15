// src/node/handlers/src/NodeMessageRouter.cpp
#include "node/handlers/include/NodeMessageRouter.hpp"
#include "node/handlers/include/NodeSigningHandler.hpp"
#include "types/MessageTypes.hpp"
#include "common/utils/logger/Logger.hpp"

namespace mpc_engine::node::handlers
{
    bool NodeMessageRouter::Initialize() 
    {
        if (initialized) {
            return true;
        }

        LOG_INFO("NodeMessageRouter", "Initializing Node Message Router...");

        // 모든 핸들러를 nullptr로 초기화
        handlers_[static_cast<size_t>(MessageType::SIGNING_REQUEST)] = NodeHandleSigningRequest;

        initialized = true;
        LOG_INFO("NodeMessageRouter", "Node Message Router initialized successfully");
        return true;
    }

    std::unique_ptr<CoordinatorNodeMessage> NodeMessageRouter::ProcessMessage(const CoordinatorNodeMessage* request) 
    {
        if (!initialized) {
            LOG_ERROR("NodeMessageRouter", "NodeMessageRouter not initialized");
            return nullptr;
        }

        if (!request) {
            LOG_ERROR("NodeMessageRouter", "Invalid request pointer");
            return nullptr;
        }

        int32_t index = request->message_type();
        if (index >= static_cast<int32_t>(MessageType::MAX_MESSAGE_TYPE)) {
            LOG_ERRORF("NodeMessageRouter", "Invalid message type: %d", index);
            return nullptr;
        }

        if (!handlers_[index]) {
            LOG_ERRORF("NodeMessageRouter", "No handler for message type: %d", index);
            return nullptr;
        }

        LOG_DEBUGF("NodeMessageRouter", "Processing message type: %d", index);
        return handlers_[index](request);
    }
}