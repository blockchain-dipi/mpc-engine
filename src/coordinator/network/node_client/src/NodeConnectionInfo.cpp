#include "coordinator/network/node_client/include/NodeConnectionInfo.hpp"
#include "common/utils/SocketUtils.hpp"
#include <sstream>

namespace mpc_engine::coordinator::network
{
    void NodeConnectionInfo::Initialize(socket_t sock, const std::string& addr, uint16_t p) {
        node_socket = sock;
        node_address = addr;
        node_port = p;
        status = ConnectionStatus::CONNECTED;
        connection_attempt_time = utils::GetCurrentTimeMs();
        last_successful_communication = utils::GetCurrentTimeMs();
        failed_attempts = 0;
    }

    bool NodeConnectionInfo::IsValid() const {
        return node_socket != INVALID_SOCKET_VALUE && 
               !node_address.empty() && 
               node_port > 0 &&
               !node_id.empty();
    }

    bool NodeConnectionInfo::IsConnected() const {
        return status == ConnectionStatus::CONNECTED;
    }

    std::string NodeConnectionInfo::GetEndpoint() const {
        return node_address + ":" + std::to_string(node_port);
    }

    std::string NodeConnectionInfo::ToString() const {
        std::ostringstream oss;
        oss << "NodeConnection[id=" << node_id 
            << ", platform=" << mpc_engine::node::ToString(platform)
            << ", endpoint=" << GetEndpoint()
            << ", shard=" << shard_index
            << ", status=";
        
        switch (status.load()) {
            case ConnectionStatus::CONNECTED:
                oss << "CONNECTED";
                break;
            case ConnectionStatus::DISCONNECTED:
                oss << "DISCONNECTED";
                break;
            case ConnectionStatus::CONNECTING:
                oss << "CONNECTING";
                break;
            case ConnectionStatus::ERROR:
                oss << "ERROR";
                break;
        }
        
        oss << ", success_rate=" << GetSuccessRate() << "%]";
        return oss.str();
    }

    uint64_t NodeConnectionInfo::GetConnectionAge() const {
        if (connection_attempt_time == 0) {
            return 0;
        }
        return utils::GetCurrentTimeMs() - connection_attempt_time;
    }

    double NodeConnectionInfo::GetSuccessRate() const {
        if (total_requests_sent == 0) {
            return 0.0;
        }
        return (static_cast<double>(successful_responses) / total_requests_sent) * 100.0;
    }
}