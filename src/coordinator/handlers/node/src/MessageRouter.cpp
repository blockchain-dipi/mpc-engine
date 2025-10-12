// src/coordinator/handlers/node/src/MessageRouter.cpp
#include "coordinator/handlers/node/include/MessageRouter.hpp"
#include "protocols/coordinator_node/include/SigningProtocol.hpp"
#include "coordinator/handlers/node/include/SigningHandler.hpp"
#include <iostream>
#include <sstream>

namespace mpc_engine::coordinator::handlers::node
{
    bool MessageRouter::Initialize() 
    {
        if (initialized) 
        {
            return true;        
        }
    
        handlers_[static_cast<size_t>(MessageType::SIGNING_REQUEST)] = HandleSigningRequest;
    
        initialized = true;
        return true;
    }

    std::unique_ptr<BaseResponse> MessageRouter::ProcessMessage(MessageType type, const BaseRequest* request) 
    {
        if (!initialized) 
        {
            std::cerr << "MessageRouter not initialized" << std::endl;
            return nullptr;
        }

        size_t index = static_cast<size_t>(type);

        if (index >= static_cast<size_t>(MessageType::MAX_MESSAGE_TYPE)) 
        {
            std::cerr << "Invalid message type: " << index << std::endl;
            return nullptr;
        }

        if (!handlers_[index]) 
        {
            std::cerr << "No handler for message type: " << index << std::endl;
            return nullptr;
        }

        return handlers_[index](request);
    }
} // namespace mpc_engine::coordinator::handlers::node