// src/coordinator/handlers/node/include/ProtocolRouter.hpp
#pragma once

#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <string>
#include <cstdint>
#include <functional>
#include <memory>

using namespace mpc_engine::protocol::coordinator_node;

namespace mpc_engine::coordinator::handlers::node
{
    // 핸들러 함수 타입 정의
    using ProtocolHandler = std::function<std::unique_ptr<BaseResponse>(const BaseRequest*)>;
    
    class ProtocolRouter 
    {
    private:
        ProtocolRouter() = default;
        bool initialized = false;

    public:
        static ProtocolRouter& Instance() 
        {
            static ProtocolRouter instance;
            return instance;
        }
        bool Initialize();
        std::unique_ptr<BaseResponse> ProcessMessage(MessageType type, const BaseRequest* request);
    
    private:
        std::array<ProtocolHandler, static_cast<size_t>(MessageType::MAX_MESSAGE_TYPE)> handlers;
    };
} // namespace mpc_engine::protocol