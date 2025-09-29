// src/common/types/BasicTypes.hpp
#pragma once
#include <cstdint>
#include <string>

namespace mpc_engine 
{
    using socket_t = int;
    #define INVALID_SOCKET_VALUE -1
    #define CLOSE_SOCKET close
    
    constexpr uint32_t DEFAULT_BUFFER_SIZE = 4096;
    constexpr uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;
    constexpr uint32_t DEFAULT_TIMEOUT_MS = 5000;
    constexpr uint32_t DEFAULT_TCP_TIMEOUT_MS = 5000;  // 추가
    
    enum class ConnectionStatus 
    {
        DISCONNECTED = 0,
        CONNECTING = 1,
        CONNECTED = 2,
        ERROR = 3
    };
    
    enum class NetworkError 
    {
        NONE = 0,
        CONNECTION_FAILED = 1,
        SEND_FAILED = 2,
        RECEIVE_FAILED = 3,
        TIMEOUT = 4,
        INVALID_DATA = 5,
        // NodeTcpClient에서 사용하는 에러 추가
        CONNECTION_ERROR = 6,
        SEND_ERROR = 7,
        SOCKET_CREATE_ERROR = 8,
        INVALID_ADDRESS = 9
    };
}