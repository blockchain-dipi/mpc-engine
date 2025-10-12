// src/node/handlers/src/NodeMessageRouter.cpp
#include "node/handlers/include/NodeMessageRouter.hpp"
#include "node/handlers/include/NodeSigningHandler.hpp"
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
        handlers_[static_cast<size_t>(MessageType::SIGNING_REQUEST)] = NoeHandleSigningRequest;

        initialized = true;
        std::cout << "Node Message Router initialized successfully" << std::endl;
        return true;
    }

    std::unique_ptr<BaseResponse> NodeMessageRouter::ProcessMessage(MessageType type, const BaseRequest* request) 
    {
        if (!initialized) {
            std::cerr << "NodeMessageRouter not initialized" << std::endl;
            return nullptr;
        }

        if (!request) {
            std::cerr << "Invalid request pointer" << std::endl;
            return nullptr;
        }

        size_t index = static_cast<size_t>(type);

        // 범위 체크
        if (index >= static_cast<size_t>(MessageType::MAX_MESSAGE_TYPE)) {
            std::cerr << "Invalid message type: " << index << std::endl;
            return nullptr;
        }

        // 핸들러 존재 체크
        if (!handlers_[index]) {
            std::cerr << "No handler for message type: " << index << std::endl;
            return nullptr;
        }

        std::cout << "Processing message type: " << static_cast<uint32_t>(type) << std::endl;

        // O(1) 핸들러 호출
        return handlers_[index](request);
    }
}