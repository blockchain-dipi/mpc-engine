// src/coordinator/CoordinatorServer.hpp
#pragma once
#include "coordinator/network/node_client/include/NodeTcpClient.hpp"
#include "coordinator/network/wallet_server/include/WalletServerManager.hpp"
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include "protocols/coordinator_wallet/include/WalletProtocolTypes.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace mpc_engine::coordinator
{
    using namespace protocol::coordinator_node;
    using namespace mpc_engine::network::tls;

    struct CoordinatorStats 
    {
        uint32_t total_nodes = 0;
        uint32_t connected_nodes = 0;
        uint32_t ready_nodes = 0;
        uint32_t error_nodes = 0;
        uint64_t uptime_seconds = 0;
        uint64_t last_update_time = 0;
    };

    class CoordinatorServer 
    {
    private:
        // Node 클라이언트 관리
        std::unordered_map<std::string, std::unique_ptr<network::NodeTcpClient>> node_clients;
        mutable std::mutex nodes_mutex;

        // Wallet Server Manager (Singleton 참조)
        network::WalletServerManager& wallet_manager_ = network::WalletServerManager::Instance();

        std::atomic<bool> is_running{false};
        std::atomic<bool> is_initialized{false};

        uint64_t start_time = 0;

        static std::unique_ptr<CoordinatorServer> instance;
        static std::mutex instance_mutex;

    public:
        CoordinatorServer();
        ~CoordinatorServer();

        static CoordinatorServer& Instance();

        bool Initialize();
        bool Start();
        void Stop();
        bool IsRunning() const;

        bool RegisterNode(const std::string& node_id,
            PlatformType platform,
            const std::string& address,
            uint16_t port,
            uint32_t shard_index = 0);
        
        void UnregisterNode(const std::string& node_id);
        bool HasNode(const std::string& node_id) const;

        bool ConnectToNode(const std::string& node_id);
        void DisconnectFromNode(const std::string& node_id);
        bool IsNodeConnected(const std::string& node_id) const;
        void DisconnectAllNodes();

        std::unique_ptr<BaseResponse> SendToNode(const std::string& node_id, const BaseRequest* request);        
        bool BroadcastToNodes(const std::vector<std::string>& node_ids, const BaseRequest* request);        
        bool BroadcastToAllConnectedNodes(const BaseRequest* request);

        std::vector<std::string> GetConnectedNodeIds() const;
        std::vector<std::string> GetReadyNodeIds() const;
        std::vector<std::string> GetAllNodeIds() const;
        ConnectionStatus GetNodeStatus(const std::string& node_id) const;
        size_t GetConnectedNodeCount() const;
        size_t GetTotalNodeCount() const;

        std::string GetNodeAddress(const std::string& node_id) const;
        PlatformType GetNodePlatform(const std::string& node_id) const;
        uint32_t GetNodeShardIndex(const std::string& node_id) const;
        std::string GetNodeEndpoint(const std::string& node_id) const;

        CoordinatorStats GetStats() const;

        std::vector<std::string> GetNodesByPlatform(PlatformType platform) const;
        std::vector<std::string> GetNodesByStatus(ConnectionStatus status) const;
        std::vector<std::string> GetNodesByShardIndex(uint32_t shard_index) const;

        bool InitializeWalletServer(const std::string& wallet_url, const std::string& auth_token);

        // std::unique_ptr<protocol::coordinator_wallet::WalletSigningResponse> 
        //     SendToWallet(const protocol::coordinator_wallet::WalletSigningRequest& request);

        bool IsWalletServerInitialized() const;

    private:
        network::NodeTcpClient* FindNodeClientInternal(const std::string& node_id) const;
        void OnNodeStatusChanged(const std::string& node_id, ConnectionStatus status);
    };
} // namespace mpc_engine::coordinator