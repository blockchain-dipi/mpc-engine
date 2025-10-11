// src/coordinator/handlers/node/include/MessageRouter.hpp
#pragma once

#include "types/MessageTypes.hpp"
#include <string>
#include <cstdint>
#include <functional>
#include <memory>

namespace mpc_engine::coordinator::handlers::node
{
    using namespace mpc_engine::protocol::coordinator_node;
    
    // 핸들러 함수 타입 정의
    using MessageHandler = std::function<std::unique_ptr<BaseResponse>(const BaseRequest*)>;
    
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
        std::unique_ptr<BaseResponse> ProcessMessage(MessageType type, const BaseRequest* request);
    
    private:
        std::array<MessageHandler, static_cast<size_t>(MessageType::MAX_MESSAGE_TYPE)> handlers;
    };
} // namespace mpc_engine::protocol