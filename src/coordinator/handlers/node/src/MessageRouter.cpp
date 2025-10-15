// src/coordinator/handlers/node/src/MessageRouter.cpp
#include "coordinator/handlers/node/include/MessageRouter.hpp"
#include "coordinator/handlers/node/include/SigningHandler.hpp"
#include "common/utils/logger/Logger.hpp"

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
            LOG_ERROR("MessageRouter", "MessageRouter not initialized");
            return nullptr;
        }

        int32_t index = request->message_type();
        if (index >= static_cast<int32_t>(mpc_engine::MessageType::MAX_MESSAGE_TYPE)) 
        {
            LOG_ERRORF("MessageRouter", "Invalid message type: %d", index);
            return nullptr;
        }

        if (!handlers_[index]) 
        {
            LOG_ERRORF("MessageRouter", "No handler for message type: %d", index);
            return nullptr;
        }

        return handlers_[index](request);
    }
} // namespace mpc_engine::coordinator::handlers::node