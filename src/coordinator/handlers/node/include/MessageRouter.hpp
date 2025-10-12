// src/coordinator/handlers/node/include/MessageRouter.hpp
#pragma once
#include "types/MessageTypes.hpp"
#include "proto/coordinator_node/generated/common.pb.h"
#include "proto/coordinator_node/generated/message.pb.h"
#include <string>
#include <cstdint>
#include <functional>
#include <memory>

namespace mpc_engine::coordinator::handlers::node
{
    using namespace mpc_engine::proto::coordinator_node;
    
    using MessageHandler = std::function<std::unique_ptr<CoordinatorNodeMessage>(const CoordinatorNodeMessage*)>;
    
    
    class MessageRouter 
    {
    private:
        MessageRouter() = default;
        bool initialized = false;

    public:
        static MessageRouter& Instance() 
        {
            static MessageRouter instance;
            return instance;
        }
        bool Initialize();
        std::unique_ptr<CoordinatorNodeMessage> ProcessMessage(const CoordinatorNodeMessage* request);
    
    private:
        std::array<MessageHandler, static_cast<size_t>(mpc_engine::MessageType::MAX_MESSAGE_TYPE)> handlers_{};
    };
} // namespace mpc_engine::coordinator::handlers::node