// src/types/MessageTypes.hpp
#pragma once
#include <cstdint>

namespace mpc_engine
{
    enum class MessageType : uint32_t 
    {
        SIGNING_REQUEST = 0,
        MAX_MESSAGE_TYPE  // 항상 마지막
    };
}