// src/common/utils/socket/SocketUtils.hpp
#pragma once
#include "types/BasicTypes.hpp"
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

    // 정확한 송수신 함수들
    enum class SocketIOResult {
        SUCCESS = 0,
        CONNECTION_CLOSED = 1,    // 정상 연결 종료 (recv: 0 반환)
        INTERRUPTED = 2,          // 시그널 인터럽트 (EINTR)
        TIMEOUT = 3,              // 타임아웃 (EAGAIN/EWOULDBLOCK)
        CONNECTION_ERROR = 4,     // 연결 에러 (EPIPE, ECONNRESET 등)
        UNKNOWN_ERROR = 5         // 기타 에러
    };

    // 정확히 length 바이트를 수신 (부분 수신 방어)
    SocketIOResult ReceiveExact(socket_t sock, void* buffer, size_t length, size_t* bytes_received = nullptr);
    
    // 정확히 length 바이트를 송신 (부분 송신 방어)
    SocketIOResult SendExact(socket_t sock, const void* data, size_t length, size_t* bytes_sent = nullptr);
    
    // SocketIOResult를 문자열로 변환
    const char* SocketIOResultToString(SocketIOResult result);
    
    // SocketIOResult가 치명적 에러인지 확인
    bool IsFatalError(SocketIOResult result);
    
    // SocketIOResult가 재시도 가능한지 확인
    bool IsRetryable(SocketIOResult result);
}