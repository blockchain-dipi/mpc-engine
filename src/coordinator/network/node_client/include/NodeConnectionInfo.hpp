#pragma once
#include "common/types/BasicTypes.hpp"
#include "node/NodeServer.hpp"
#include <atomic>

namespace mpc_engine::coordinator::network 
{
    struct NodeConnectionInfo {
        socket_t node_socket = INVALID_SOCKET_VALUE;
        std::string node_address;
        uint16_t node_port = 0;
        std::string node_id;
        
        mpc_engine::node::NodePlatformType platform;
        uint32_t shard_index = 0;
        
        std::atomic<ConnectionStatus> status{ConnectionStatus::DISCONNECTED};
        uint64_t connection_attempt_time = 0;
        uint64_t last_successful_communication = 0;
        uint32_t failed_attempts = 0;
        uint32_t connection_timeout_ms = DEFAULT_TCP_TIMEOUT_MS;
        
        uint32_t total_requests_sent = 0;
        uint32_t successful_responses = 0;
        uint32_t failed_responses = 0;
        
        void Initialize(socket_t sock, const std::string& addr, uint16_t p);
        bool IsValid() const;
        bool IsConnected() const;
        std::string GetEndpoint() const;
        std::string ToString() const;
        uint64_t GetConnectionAge() const;
        double GetSuccessRate() const;
    };
}