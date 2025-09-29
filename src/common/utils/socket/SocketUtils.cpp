// src/common/utils/socket/SocketUtils.cpp
#include "SocketUtils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

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
}