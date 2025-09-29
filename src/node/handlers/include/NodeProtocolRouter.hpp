// src/node/handlers/include/NodeProtocolRouter.hpp
#pragma once

#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <string>
#include <cstdint>
#include <functional>
#include <memory>
#include <array>

using namespace mpc_engine::protocol::coordinator_node;

namespace mpc_engine::node::handlers
{
    // 핸들러 함수 타입 정의
    using NodeProtocolHandler = std::function<std::unique_ptr<BaseResponse>(const BaseRequest*)>;
    
    class NodeProtocolRouter 
    {
    private:
        NodeProtocolRouter() = default;
        bool initialized = false;

    public:
        static NodeProtocolRouter& Instance() 
        {
            static NodeProtocolRouter instance;
            return instance;
        }
        
        // 초기화 - 모든 핸들러 등록
        bool Initialize();
        std::unique_ptr<BaseResponse> ProcessMessage(MessageType type, const BaseRequest* request);
       
    private:
        std::array<NodeProtocolHandler, static_cast<size_t>(MessageType::MAX_MESSAGE_TYPE)> handlers;
    };
} // namespace mpc_engine::node::handlers