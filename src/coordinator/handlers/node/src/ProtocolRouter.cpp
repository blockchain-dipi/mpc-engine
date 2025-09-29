// src/coordinator/handlers/node/src/ProtocolRouter.cpp
#include "coordinator/handlers/node/include/ProtocolRouter.hpp"
#include "protocols/coordinator_node/include/SigningProtocol.hpp"
#include <iostream>
#include <sstream>

namespace mpc_engine::coordinator::handlers::node
{
    bool ProtocolRouter::Initialize() 
    {
        if (initialized) 
        {
            return true;        
        }
    
        handlers[static_cast<size_t>(MessageType::SIGNING_REQUEST)] = HandleSigningRequest;
    
        initialized = true;
        return true;
    }

    std::unique_ptr<BaseResponse> ProtocolRouter::ProcessMessage(MessageType type, const BaseRequest* request) 
    {
        if (!initialized) 
        {
            std::cerr << "ProtocolRouter not initialized" << std::endl;
            return nullptr;
        }

        size_t index = static_cast<size_t>(type);

        // 범위 체크
        if (index >= static_cast<size_t>(MessageType::MAX_MESSAGE_TYPE)) 
        {
            std::cerr << "Invalid message type: " << index << std::endl;
            return nullptr;
        }

        // 핸들러 존재 체크
        if (!handlers[index]) 
        {
            std::cerr << "No handler for message type: " << index << std::endl;
            return nullptr;
        }

        // O(1) 핸들러 호출
        return handlers[index](request);
    }
} // namespace mpc_engine::protocol