// src/node/network/include/NodeConnectionInfo.hpp
#pragma once
#include "common/types/BasicTypes.hpp"
#include <string>

namespace mpc_engine::node::network
{
    struct NodeConnectionInfo 
    {
        socket_t coordinator_socket = INVALID_SOCKET_VALUE;
        std::string coordinator_address;
        uint16_t coordinator_port = 0;
        std::string session_id;
        uint64_t session_start_time = 0;
        uint64_t last_activity_time = 0;
        uint32_t total_requests_handled = 0;
        uint32_t successful_requests = 0;
        ConnectionStatus status = ConnectionStatus::DISCONNECTED;

        void Initialize(socket_t sock, const std::string& addr, uint16_t port);
        bool IsValid() const;
        bool IsActive() const;
        std::string ToString() const;
    };
}