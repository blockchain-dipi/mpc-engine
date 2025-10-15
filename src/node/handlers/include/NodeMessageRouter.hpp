// src/node/handlers/include/NodeMessageRouter.hpp
#pragma once

#include "types/MessageTypes.hpp"
#include "proto/coordinator_node/generated/message.pb.h"
#include <functional>
#include <memory>
#include <array>

namespace mpc_engine::node::handlers
{
    using namespace mpc_engine::proto::coordinator_node;

    using NodeMessageHandler = std::function<std::unique_ptr<CoordinatorNodeMessage>(const CoordinatorNodeMessage*)>;
    
    class NodeMessageRouter 
    {
    private:
        NodeMessageRouter() = default;
        bool initialized = false;

    public:
        static NodeMessageRouter& Instance() 
        {
            static NodeMessageRouter instance;
            return instance;
        }
        
        bool Initialize();
        std::unique_ptr<CoordinatorNodeMessage> ProcessMessage(const CoordinatorNodeMessage* request);
       
    private:
        std::array<NodeMessageHandler, static_cast<size_t>(MessageType::MAX_MESSAGE_TYPE)> handlers_{};
    };
} // namespace mpc_engine::node::handlers