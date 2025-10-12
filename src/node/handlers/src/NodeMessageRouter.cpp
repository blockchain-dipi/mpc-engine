// src/node/handlers/src/NodeMessageRouter.cpp
#include "node/handlers/include/NodeMessageRouter.hpp"
#include "node/handlers/include/NodeSigningHandler.hpp"
#include "types/MessageTypes.hpp"
#include <iostream>

namespace mpc_engine::node::handlers
{
    bool NodeMessageRouter::Initialize() 
    {
        if (initialized) {
            return true;
        }

        std::cout << "Initializing Node Message Router..." << std::endl;

        // 모든 핸들러를 nullptr로 초기화
        handlers_[static_cast<size_t>(MessageType::SIGNING_REQUEST)] = NodeHandleSigningRequest;

        initialized = true;
        std::cout << "Node Message Router initialized successfully" << std::endl;
        return true;
    }

    std::unique_ptr<CoordinatorNodeMessage> NodeMessageRouter::ProcessMessage(const CoordinatorNodeMessage* request) 
    {
        if (!initialized) {
            std::cerr << "NodeMessageRouter not initialized" << std::endl;
            return nullptr;
        }

        if (!request) {
            std::cerr << "Invalid request pointer" << std::endl;
            return nullptr;
        }

        int32_t index = request->message_type();
        if (index >= static_cast<int32_t>(MessageType::MAX_MESSAGE_TYPE)) {
            std::cerr << "Invalid message type: " << index << std::endl;
            return nullptr;
        }

        if (!handlers_[index]) {
            std::cerr << "No handler for message type: " << index << std::endl;
            return nullptr;
        }

        std::cout << "Processing message type: " << index << std::endl;
        return handlers_[index](request);
    }
}