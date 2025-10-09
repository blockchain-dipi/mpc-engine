// src/node/network/include/NodeConnectionInfo.hpp
#pragma once
#include "common/types/BasicTypes.hpp"
#include "common/network/tls/include/TlsConnection.hpp"
#include <string>
#include <memory>

namespace mpc_engine::node::network
{
    using namespace mpc_engine::network::tls;
    
    struct NodeConnectionInfo 
    {
        std::unique_ptr<TlsConnection> tls_connection;

        std::string coordinator_address;
        uint16_t coordinator_port = 0;
        uint64_t connection_start_time = 0;
        uint64_t last_activity_time = 0;
        uint32_t total_requests_handled = 0;
        uint32_t total_responses_sent = 0;
        ConnectionStatus status = ConnectionStatus::DISCONNECTED;

        void InitializeWithTls(const std::string& addr, uint16_t port, std::unique_ptr<TlsConnection> tls_conn);
        bool IsValid() const;
        bool IsActive() const;
        std::string ToString() const;
        TlsConnection& GetTlsConnection() const;

        // 기본 생성자
        NodeConnectionInfo() = default;

        // 복사 생성자는 삭제 (unique_ptr 때문에)
        NodeConnectionInfo(const NodeConnectionInfo&) = delete;
        NodeConnectionInfo& operator=(const NodeConnectionInfo&) = delete;
        
        // Move 생성자는 허용
        NodeConnectionInfo(NodeConnectionInfo&&) = default;
        NodeConnectionInfo& operator=(NodeConnectionInfo&&) = default;
        
        void Disconnect() {
            status = ConnectionStatus::DISCONNECTED;
            if (tls_connection) {
                tls_connection->Close();
                tls_connection.reset();
            }
        }

        // 콜백용 복사 가능한 정보만 추출
        struct DisconnectionInfo {
            std::string coordinator_address;
            uint16_t coordinator_port;
            uint64_t connection_start_time;
            uint64_t last_activity_time;
            uint32_t total_requests_handled;
            uint32_t total_responses_sent;
            ConnectionStatus status;
        };

        DisconnectionInfo GetDisconnectionInfo() const {
            return DisconnectionInfo{
                coordinator_address,
                coordinator_port,
                connection_start_time,
                last_activity_time,
                total_requests_handled,
                total_responses_sent,
                status
            };
        }
    };
}