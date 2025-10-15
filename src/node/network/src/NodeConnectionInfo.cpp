// src/node/network/src/NodeConnectionInfo.cpp
#include "node/network/include/NodeConnectionInfo.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/utils/logger/Logger.hpp"
#include <stdexcept>

namespace mpc_engine::node::network
{
    void NodeConnectionInfo::InitializeWithTls(const std::string& addr, uint16_t port, std::unique_ptr<TlsConnection> tls_conn) 
    {
        tls_connection = std::move(tls_conn);
        coordinator_address = addr;
        coordinator_port = port;
        connection_start_time = utils::GetCurrentTimeMs();
        last_activity_time = connection_start_time;
        status = ConnectionStatus::CONNECTED;
    }

    bool NodeConnectionInfo::IsValid() const 
    {
        return tls_connection != nullptr && !coordinator_address.empty() && coordinator_port > 0;
    }

    bool NodeConnectionInfo::IsActive() const 
    {
        return status == ConnectionStatus::CONNECTED && IsValid();
    }

    std::string NodeConnectionInfo::ToString() const 
    {
        return coordinator_address + ":" + std::to_string(coordinator_port);
    }
    
    TlsConnection& NodeConnectionInfo::GetTlsConnection() const
    {
        if (!tls_connection) {
            LOG_ERROR("NodeConnectionInfo", "TLS connection is null");
            throw std::runtime_error("TLS connection is null");
        }
        return *tls_connection;
    }
}