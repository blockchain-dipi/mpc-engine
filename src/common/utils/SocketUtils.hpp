// src/common/utils/SocketUtils.hpp
#pragma once
#include "common/types/BasicTypes.hpp"
#include <string>
#include <chrono>

namespace mpc_engine::utils 
{
    bool SetSocketNonBlocking(socket_t sock);
    bool SetSocketReuseAddr(socket_t sock);
    bool SetSocketNoDelay(socket_t sock);
    
    struct KeepAliveConfig {
        bool enabled = true;
        uint32_t idle_seconds = 10;
        uint32_t interval_seconds = 5;
        uint32_t probe_count = 3;
    };
    bool SetSocketKeepAlive(socket_t sock, const KeepAliveConfig& config);
    
    bool SetSocketSendTimeout(socket_t sock, uint32_t timeout_ms);
    bool SetSocketRecvTimeout(socket_t sock, uint32_t timeout_ms);
    bool SetSocketBufferSize(socket_t sock, uint32_t recv_size, uint32_t send_size);
    
    std::string GetErrorString(int error_code);
    void CloseSocket(socket_t sock);
    
    inline uint64_t GetCurrentTimeMs() 
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}