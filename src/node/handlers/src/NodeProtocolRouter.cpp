// src/node/handlers/src/NodeProtocolRouter.cpp
#include "node/handlers/include/NodeProtocolRouter.hpp"
#include "node/handlers/src/NodeSigningProtocol.cpp"
#include <iostream>

namespace mpc_engine::node::handlers
{
    bool NodeProtocolRouter::Initialize() 
    {
        if (initialized) {
            return true;
        }

        std::cout << "Initializing Node Protocol Router..." << std::endl;

        // 모든 핸들러를 nullptr로 초기화
        handlers[static_cast<size_t>(MessageType::SIGNING_REQUEST)] = HandleSigningRequest;

        initialized = true;
        std::cout << "Node Protocol Router initialized successfully" << std::endl;
        return true;
    }

    std::unique_ptr<BaseResponse> NodeProtocolRouter::ProcessMessage(MessageType type, const BaseRequest* request) 
    {
        if (!initialized) {
            std::cerr << "NodeProtocolRouter not initialized" << std::endl;
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
        if (!handlers[index]) {
            std::cerr << "No handler for message type: " << index << std::endl;
            return nullptr;
        }

        std::cout << "Processing message type: " << static_cast<uint32_t>(type) << std::endl;

        // O(1) 핸들러 호출
        return handlers[index](request);
    }
}