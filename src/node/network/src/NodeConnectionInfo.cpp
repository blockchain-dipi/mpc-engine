// src/node/network/src/NodeConnectionInfo.cpp
#include "node/network/include/NodeConnectionInfo.hpp"
#include "common/utils/SocketUtils.hpp"

namespace mpc_engine::node::network
{
    void NodeConnectionInfo::Initialize(socket_t sock, const std::string& addr, uint16_t port) 
    {
        coordinator_socket = sock;
        coordinator_address = addr;
        coordinator_port = port;
        session_start_time = utils::GetCurrentTimeMs();
        last_activity_time = session_start_time;
        status = ConnectionStatus::CONNECTED;
    }

    bool NodeConnectionInfo::IsValid() const 
    {
        return coordinator_socket != INVALID_SOCKET_VALUE && !coordinator_address.empty() && coordinator_port > 0;
    }

    bool NodeConnectionInfo::IsActive() const 
    {
        return status == ConnectionStatus::CONNECTED && IsValid();
    }

    std::string NodeConnectionInfo::ToString() const 
    {
        return coordinator_address + ":" + std::to_string(coordinator_port);
    }
}