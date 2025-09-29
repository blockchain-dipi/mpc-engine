// src/coordinator/CoordinatorServer.cpp
#include "coordinator/CoordinatorServer.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <algorithm>
#include <iostream>

namespace mpc_engine::coordinator
{
    std::unique_ptr<CoordinatorServer> CoordinatorServer::instance = nullptr;
    std::mutex CoordinatorServer::instance_mutex;

    CoordinatorServer& CoordinatorServer::Instance() 
    {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (!instance) {
            instance = std::make_unique<CoordinatorServer>();
        }
        return *instance;
    }

    CoordinatorServer::CoordinatorServer() 
    {
        start_time = utils::GetCurrentTimeMs();
    }

    CoordinatorServer::~CoordinatorServer() 
    {
        Stop();
    }

    bool CoordinatorServer::Initialize() 
    {
        if (is_initialized.load()) 
        {
            return true;
        }

        is_initialized = true;
        return true;
    }

    bool CoordinatorServer::Start() 
    {
        if (!is_initialized.load() || is_running.load()) 
        {
            return false;
        }

        is_running = true;
        std::cout << "Coordinator server started" << std::endl;
        return true;
    }

    void CoordinatorServer::Stop() 
    {
        if (!is_running.load()) 
        {
            return;
        }
    
        is_running = false;
        DisconnectAllNodes();

        std::cout << "Coordinator server stopped" << std::endl;
    }

    bool CoordinatorServer::IsRunning() const 
    {
        return is_running.load();
    }

    bool CoordinatorServer::RegisterNode(
        const std::string& node_id,
        mpc_engine::node::NodePlatformType platform,
        const std::string& address,
        uint16_t port,
        uint32_t shard_index) 
    {
        if (node_id.empty() || address.empty() || port == 0) 
        {
            std::cerr << "Invalid node parameters" << std::endl;
            return false;
        }

        std::lock_guard<std::mutex> lock(nodes_mutex);
        
        if (node_clients.find(node_id) != node_clients.end()) 
        {
            std::cerr << "Node already registered: " << node_id << std::endl;
            return false;
        }

        std::unique_ptr<network::NodeTcpClient> node_client 
        = std::make_unique<network::NodeTcpClient>(node_id, address, port, platform, shard_index);
        
        node_client->SetConnectedCallback([this](const network::NodeConnectionInfo& info) 
        {
            OnNodeStatusChanged(info.node_id, ConnectionStatus::CONNECTED);
        });
        
        node_client->SetDisconnectedCallback([this](const network::NodeConnectionInfo& info) 
        {
            OnNodeStatusChanged(info.node_id, ConnectionStatus::DISCONNECTED);
        });
        
        node_clients[node_id] = std::move(node_client);
        
        std::cout << "Node registered: " << node_id << " at " << address << ":" << port 
                  << " (platform: " << mpc_engine::node::ToString(platform) 
                  << ", shard: " << shard_index << ")" << std::endl;
        
        return true;
    }

    void CoordinatorServer::UnregisterNode(const std::string& node_id) 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        
        std::unordered_map<std::string, std::unique_ptr<network::NodeTcpClient>>::const_iterator it = node_clients.find(node_id);
        if (it != node_clients.end()) 
        {
            it->second->Disconnect();
            node_clients.erase(it);
            std::cout << "Node unregistered: " << node_id << std::endl;
        }
    }

    bool CoordinatorServer::HasNode(const std::string& node_id) const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        return node_clients.find(node_id) != node_clients.end();
    }

    bool CoordinatorServer::ConnectToNode(const std::string& node_id) 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        if (!client) {
            std::cerr << "Node not found: " << node_id << std::endl;
            return false;
        }

        return client->Connect();
    }

    void CoordinatorServer::DisconnectFromNode(const std::string& node_id) 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        if (client) {
            client->Disconnect();
        }
    }

    bool CoordinatorServer::IsNodeConnected(const std::string& node_id) const 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        return client && client->IsConnected();
    }

    void CoordinatorServer::DisconnectAllNodes() 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);

        for (const std::pair<const std::string, std::unique_ptr<network::NodeTcpClient>>& entry : node_clients)
        {
            if (entry.second) 
            {
                entry.second->Disconnect();
            }
        }
    }

    std::unique_ptr<BaseResponse> CoordinatorServer::SendToNode(const std::string& node_id, const BaseRequest* request) 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        if (!client) 
        {
            std::cerr << "Node not found: " << node_id << std::endl;
            return nullptr;
        }

        return client->SendRequest(request);
    }

    bool CoordinatorServer::BroadcastToNodes(const std::vector<std::string>& node_ids, const BaseRequest* request) 
    {
        bool all_success = true;

        for (const std::string& node_id : node_ids) 
        {
            std::unique_ptr<BaseResponse> response = SendToNode(node_id, request);
            if (!response || !response->success) 
            {
                all_success = false;
                std::cerr << "Failed to send request to node: " << node_id << std::endl;
            }
        }
        
        return all_success;
    }

    bool CoordinatorServer::BroadcastToAllConnectedNodes(const BaseRequest* request) 
    {
        std::vector<std::string> connected_nodes = GetConnectedNodeIds();
        return BroadcastToNodes(connected_nodes, request);
    }

    std::vector<std::string> CoordinatorServer::GetConnectedNodeIds() const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const std::pair<const std::string, std::unique_ptr<network::NodeTcpClient>>& entry : node_clients) 
        {
            if (entry.second && entry.second->IsConnected()) 
            {
                result.push_back(entry.first);
            }
        }

        return result;
    }

    std::vector<std::string> CoordinatorServer::GetReadyNodeIds() const 
    {
        return GetConnectedNodeIds(); // 일단 연결된 노드 = 준비된 노드
    }

    std::vector<std::string> CoordinatorServer::GetAllNodeIds() const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const std::pair<const std::string, std::unique_ptr<network::NodeTcpClient>>& entry : node_clients)
        {
            result.push_back(entry.first);
        }

        return result;
    }

    ConnectionStatus CoordinatorServer::GetNodeStatus(const std::string& node_id) const 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        return client ? client->GetStatus() : ConnectionStatus::DISCONNECTED;
    }

    size_t CoordinatorServer::GetConnectedNodeCount() const 
    {
        return GetConnectedNodeIds().size();
    }

    size_t CoordinatorServer::GetTotalNodeCount() const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        return node_clients.size();
    }

    std::string CoordinatorServer::GetNodeAddress(const std::string& node_id) const 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        return client ? client->GetAddress() : "";
    }

    mpc_engine::node::NodePlatformType CoordinatorServer::GetNodePlatform(const std::string& node_id) const 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        return client ? client->GetPlatform() : mpc_engine::node::NodePlatformType::UNKNOWN;
    }

    uint32_t CoordinatorServer::GetNodeShardIndex(const std::string& node_id) const 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        return client ? client->GetShardIndex() : 0;
    }

    std::string CoordinatorServer::GetNodeEndpoint(const std::string& node_id) const 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        return client ? client->GetEndpoint() : "";
    }

    CoordinatorStats CoordinatorServer::GetStats() const 
    {
        CoordinatorStats stats;
        
        std::lock_guard<std::mutex> lock(nodes_mutex);
        
        stats.total_nodes = static_cast<uint32_t>(node_clients.size());
        stats.uptime_seconds = (utils::GetCurrentTimeMs() - start_time) / 1000;
        stats.last_update_time = utils::GetCurrentTimeMs();
        
        for (const std::pair<const std::string, std::unique_ptr<network::NodeTcpClient>>& entry : node_clients)
        {
            if (entry.second && entry.second->IsConnected()) 
            {
                stats.connected_nodes++;
                stats.ready_nodes++;
            }
        }
        
        return stats;
    }

    std::vector<std::string> CoordinatorServer::GetNodesByPlatform(mpc_engine::node::NodePlatformType platform) const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const std::pair<const std::string, std::unique_ptr<network::NodeTcpClient>>& entry : node_clients)
        {
            if (entry.second && entry.second->GetPlatform() == platform) 
            {
                result.push_back(entry.first);
            }
        }

        return result;
    }

    std::vector<std::string> CoordinatorServer::GetNodesByStatus(ConnectionStatus status) const {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const std::pair<const std::string, std::unique_ptr<network::NodeTcpClient>>& entry : node_clients)
        {
            if (entry.second && entry.second->GetStatus() == status) 
            {
                result.push_back(entry.first);
            }
        }

        return result;
    }

    std::vector<std::string> CoordinatorServer::GetNodesByShardIndex(uint32_t shard_index) const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const std::pair<const std::string, std::unique_ptr<network::NodeTcpClient>>& entry : node_clients)
        {
            if (entry.second && entry.second->GetShardIndex() == shard_index) 
            {
                result.push_back(entry.first);
            }
        }

        return result;
    }

    network::NodeTcpClient* CoordinatorServer::FindNodeClientInternal(const std::string& node_id) const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        
        std::unordered_map<std::string, std::unique_ptr<network::NodeTcpClient>>::const_iterator it = node_clients.find(node_id);
        return (it != node_clients.end()) ? it->second.get() : nullptr;
    }

    void CoordinatorServer::OnNodeStatusChanged(const std::string& node_id, ConnectionStatus status) 
    {
        std::cout << "Node " << node_id << " status changed to: " << static_cast<int>(status) << std::endl;
    }
}