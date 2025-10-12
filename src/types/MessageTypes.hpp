// src/types/MessageTypes.hpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace mpc_engine::protocol::coordinator_node
{
    enum class MessageType : uint32_t 
    {
        SIGNING_REQUEST = 0,
        MAX_MESSAGE_TYPE  // 항상 마지막
    };
}