// src/node/handlers/include/NodeMessageRouter.hpp
#pragma once

#include "protocols/coordinator_node/include/BaseProtocol.hpp"
#include <string>
#include <cstdint>
#include <functional>
#include <memory>
#include <array>

namespace mpc_engine::node::handlers
{
    using namespace mpc_engine::protocol::coordinator_node;
    using NodeMessageHandler = std::function<std::unique_ptr<BaseResponse>(const BaseRequest*)>;
    
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
        
        // 초기화 - 모든 핸들러 등록
        bool Initialize();
        std::unique_ptr<BaseResponse> ProcessMessage(MessageType type, const BaseRequest* request);
       
    private:
        std::array<NodeMessageHandler, static_cast<size_t>(MessageType::MAX_MESSAGE_TYPE)> handlers_{};
    };
} // namespace mpc_engine::node::handlers