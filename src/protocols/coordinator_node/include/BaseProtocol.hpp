// src/protocols/coordinator_node/include/BaseProtocol.hpp
#pragma once
#include "types/MessageTypes.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace mpc_engine::protocol::coordinator_node
{
    struct BaseRequest 
    {
        MessageType messageType;
        std::string uid;
        std::string sendTime;

        BaseRequest(MessageType type) : messageType(type) {}
        virtual ~BaseRequest() = default;
    };

    struct BaseResponse 
    {
        MessageType messageType;
        bool success = false;
        std::string errorMessage;

        BaseResponse(MessageType type) : messageType(type) {}
        virtual ~BaseResponse() = default;
    };
}