// src/coordinator/handlers/node/src/MessageRouter.cpp
#include "coordinator/handlers/node/include/MessageRouter.hpp"
#include "coordinator/handlers/node/include/SigningHandler.hpp"
#include <iostream>

namespace mpc_engine::coordinator::handlers::node
{
    bool MessageRouter::Initialize() 
    {
        if (initialized) 
        {
            return true;        
        }
    
        handlers_[static_cast<size_t>(mpc_engine::MessageType::SIGNING_REQUEST)] = HandleSigningRequest;
    
        initialized = true;
        return true;
    }

    std::unique_ptr<CoordinatorNodeMessage> MessageRouter::ProcessMessage(const CoordinatorNodeMessage* request) 
    {
        if (!initialized) 
        {
            std::cerr << "MessageRouter not initialized" << std::endl;
            return nullptr;
        }

        int32_t index = request->message_type();
        if (index >= static_cast<int32_t>(mpc_engine::MessageType::MAX_MESSAGE_TYPE)) 
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