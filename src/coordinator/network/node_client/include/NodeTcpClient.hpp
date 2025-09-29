// src/coordinator/network/node_client/include/NodeTcpClient.hpp
#pragma once
#include "NodeConnectionInfo.hpp"
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>

namespace mpc_engine::coordinator::network
{
    using namespace protocol::coordinator_node;
    using namespace mpc_engine::node;
    
    using NodeConnectedCallback = std::function<void(const NodeConnectionInfo&)>;
    using NodeDisconnectedCallback = std::function<void(const NodeConnectionInfo&)>;
    using NodeErrorCallback = std::function<void(const NodeConnectionInfo&, NetworkError, const std::string&)>;

    class NodeTcpClient 
    {
    private:
        NodeConnectionInfo connection_info;
        mutable std::mutex client_mutex;
        
        std::atomic<uint64_t> last_used_time{0};  // 💡 추가
        
        NodeConnectedCallback connected_callback;
        NodeDisconnectedCallback disconnected_callback;
        NodeErrorCallback error_callback;

    public:
        NodeTcpClient(const std::string& node_id, 
            const std::string& address, 
            uint16_t port,
            NodePlatformType platform,
            uint32_t shard_index = 0);
        ~NodeTcpClient();

        bool Connect();
        void Disconnect();
        bool IsConnected() const;
        bool EnsureConnection();

        bool SendMessage(const NetworkMessage& message);
        bool ReceiveMessage(NetworkMessage& message);

        std::unique_ptr<BaseResponse> SendRequest(const BaseRequest* request);

        void SetConnectedCallback(NodeConnectedCallback callback);
        void SetDisconnectedCallback(NodeDisconnectedCallback callback);
        void SetErrorCallback(NodeErrorCallback callback);

        const NodeConnectionInfo& GetConnectionInfo() const;
        std::string GetNodeId() const { return connection_info.node_id; }
        std::string GetAddress() const { return connection_info.node_address; }
        uint16_t GetPort() const { return connection_info.node_port; }
        NodePlatformType GetPlatform() const { return connection_info.platform; }
        uint32_t GetShardIndex() const { return connection_info.shard_index; }
        std::string GetEndpoint() const { return connection_info.GetEndpoint(); }
        ConnectionStatus GetStatus() const { return connection_info.status; }

        std::string ToString() const { return connection_info.ToString(); }
        bool IsValid() const { return connection_info.IsValid(); }

    private:
        bool InitializeSocket();
        bool ConnectSocket();
        void CleanupSocket();

        bool SendRaw(const void* data, size_t length);
        bool ReceiveRaw(void* buffer, size_t length);

        void NotifyError(NetworkError error, const std::string& message);
        void UpdateConnectionStats(bool success);

        NetworkMessage ConvertToNetworkMessage(const BaseRequest* request);
        std::unique_ptr<BaseResponse> ConvertFromNetworkMessage(const NetworkMessage& message);
    };
}