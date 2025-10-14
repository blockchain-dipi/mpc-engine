// src/common/utils/socket/SocketUtils.cpp
#include "SocketUtils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <errno.h>

namespace mpc_engine::utils 
{
    bool SetSocketNonBlocking(socket_t sock) 
    {
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) return false;
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
    }

    bool SetSocketReuseAddr(socket_t sock) 
    {
        int opt = 1;
        return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
    }

    bool SetSocketNoDelay(socket_t sock) 
    {
        int opt = 1;
        return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == 0;
    }

    bool SetSocketKeepAlive(socket_t sock, const KeepAliveConfig& config) 
    {
        int keepalive = config.enabled ? 1 : 0;
        if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) != 0) {
            return false;
        }
        
        if (!config.enabled) return true;
        
        int idle = config.idle_seconds;
        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) != 0) {
            return false;
        }
        
        int interval = config.interval_seconds;
        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0) {
            return false;
        }
        
        int count = config.probe_count;
        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) != 0) {
            return false;
        }
        
        return true;
    }

    bool SetSocketSendTimeout(socket_t sock, uint32_t timeout_ms) 
    {
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
    }

    bool SetSocketRecvTimeout(socket_t sock, uint32_t timeout_ms) 
    {
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
    }

    bool SetSocketBufferSize(socket_t sock, uint32_t recv_size, uint32_t send_size) 
    {
        int recv_buf = recv_size;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buf, sizeof(recv_buf)) != 0) {
            return false;
        }
        
        int send_buf = send_size;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &send_buf, sizeof(send_buf)) != 0) {
            return false;
        }
        
        return true;
    }
    
    std::string GetErrorString(int error_code) 
    {
        return strerror(error_code);
    }
    
    void CloseSocket(socket_t sock) 
    {
        if (sock != INVALID_SOCKET_VALUE) {
            close(sock);
        }
    }

    SocketIOResult ReceiveExact(socket_t sock, void* buffer, size_t length, size_t* bytes_received)
    {
        if (bytes_received) {
            *bytes_received = 0;
        }

        if (length == 0) {
            return SocketIOResult::SUCCESS;
        }

        size_t total_received = 0;
        char* data = static_cast<char*>(buffer);

        while (total_received < length) {
            ssize_t received = recv(sock, 
                                   data + total_received, 
                                   length - total_received, 
                                   0);  // MSG_WAITALL 사용 안 함
            
            if (received > 0) {
                // ✅ 정상 수신
                total_received += received;
                
                if (bytes_received) {
                    *bytes_received = total_received;
                }
                
            } else if (received == 0) {
                // ✅ 연결 종료 (FIN 패킷)
                std::cerr << "[SOCKET] Connection closed by peer (received: " 
                          << total_received << "/" << length << " bytes)" << std::endl;
                
                if (bytes_received) {
                    *bytes_received = total_received;
                }
                
                return SocketIOResult::CONNECTION_CLOSED;
                
            } else {
                // ❌ 에러 발생
                int err = errno;
                
                if (err == EINTR) {
                    // ✅ 시그널 인터럽트 → 재시도
                    std::cerr << "[SOCKET] Receive interrupted by signal, retrying..." << std::endl;
                    continue;
                }
                
                if (err == EAGAIN || err == EWOULDBLOCK) {
                    // ⏱️ 타임아웃
                    std::cerr << "[SOCKET] Receive timeout (received: " 
                              << total_received << "/" << length << " bytes)" << std::endl;
                    
                    if (bytes_received) {
                        *bytes_received = total_received;
                    }
                    
                    return SocketIOResult::TIMEOUT;
                }
                
                if (err == ECONNRESET || err == EPIPE || err == ENOTCONN) {
                    // 🔌 연결 에러
                    std::cerr << "[SOCKET] Connection error: " << strerror(err) << std::endl;
                    
                    if (bytes_received) {
                        *bytes_received = total_received;
                    }
                    
                    return SocketIOResult::CONNECTION_ERROR;
                }
                
                // ❓ 알 수 없는 에러
                std::cerr << "[SOCKET] Unknown receive error: " << strerror(err) << std::endl;
                
                if (bytes_received) {
                    *bytes_received = total_received;
                }
                
                return SocketIOResult::UNKNOWN_ERROR;
            }
        }

        // ✅ 완전히 수신 완료
        return SocketIOResult::SUCCESS;
    }

    SocketIOResult SendExact(socket_t sock, const void* data, size_t length, size_t* bytes_sent)
    {
        if (bytes_sent) {
            *bytes_sent = 0;
        }

        if (length == 0) {
            return SocketIOResult::SUCCESS;
        }

        size_t total_sent = 0;
        const char* buffer = static_cast<const char*>(data);

        while (total_sent < length) {
            ssize_t sent = send(sock, 
                              buffer + total_sent, 
                              length - total_sent, 
                              MSG_NOSIGNAL);  // SIGPIPE 방지
            
            if (sent > 0) {
                // ✅ 정상 송신
                total_sent += sent;
                
                if (bytes_sent) {
                    *bytes_sent = total_sent;
                }
                
            } else if (sent == 0) {
                // ⚠️ send()가 0을 반환하는 경우는 거의 없지만, 방어 코드
                std::cerr << "[SOCKET] Send returned 0 (unusual)" << std::endl;
                continue;
                
            } else {
                // ❌ 에러 발생
                int err = errno;
                
                if (err == EINTR) {
                    // ✅ 시그널 인터럽트 → 재시도
                    std::cerr << "[SOCKET] Send interrupted by signal, retrying..." << std::endl;
                    continue;
                }
                
                if (err == EAGAIN || err == EWOULDBLOCK) {
                    // ⏱️ 송신 버퍼 가득참 → 재시도
                    std::cerr << "[SOCKET] Send buffer full, retrying..." << std::endl;
                    continue;
                }
                
                if (err == EPIPE || err == ECONNRESET || err == ENOTCONN) {
                    // 🔌 연결 에러
                    std::cerr << "[SOCKET] Connection error during send: " << strerror(err) << std::endl;
                    
                    if (bytes_sent) {
                        *bytes_sent = total_sent;
                    }
                    
                    return SocketIOResult::CONNECTION_ERROR;
                }
                
                // ❓ 알 수 없는 에러
                std::cerr << "[SOCKET] Unknown send error: " << strerror(err) << std::endl;
                
                if (bytes_sent) {
                    *bytes_sent = total_sent;
                }
                
                return SocketIOResult::UNKNOWN_ERROR;
            }
        }

        // ✅ 완전히 송신 완료
        return SocketIOResult::SUCCESS;
    }

    const char* SocketIOResultToString(SocketIOResult result)
    {
        switch (result) {
            case SocketIOResult::SUCCESS:
                return "SUCCESS";
            case SocketIOResult::CONNECTION_CLOSED:
                return "CONNECTION_CLOSED";
            case SocketIOResult::INTERRUPTED:
                return "INTERRUPTED";
            case SocketIOResult::TIMEOUT:
                return "TIMEOUT";
            case SocketIOResult::CONNECTION_ERROR:
                return "CONNECTION_ERROR";
            case SocketIOResult::UNKNOWN_ERROR:
                return "UNKNOWN_ERROR";
            default:
                return "UNKNOWN";
        }
    }

    bool IsFatalError(SocketIOResult result)
    {
        return result == SocketIOResult::CONNECTION_CLOSED ||
               result == SocketIOResult::CONNECTION_ERROR ||
               result == SocketIOResult::UNKNOWN_ERROR;
    }

    bool IsRetryable(SocketIOResult result)
    {
        return result == SocketIOResult::INTERRUPTED ||
               result == SocketIOResult::TIMEOUT;
    }
}