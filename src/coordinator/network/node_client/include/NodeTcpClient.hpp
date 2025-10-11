// src/coordinator/network/node_client/include/NodeTcpClient.hpp
#pragma once
#include "NodeConnectionInfo.hpp"
#include "types/MessageTypes.hpp"
#include "common/utils/queue/ThreadSafeQueue.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include "common/network/tls/include/TlsConnection.hpp"
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>
#include <future>
#include <unordered_map>
#include <thread>

namespace mpc_engine::coordinator::network
{
    using namespace protocol::coordinator_node;
    using namespace mpc_engine::node;
    using namespace mpc_engine::network::tls;
    
    using NodeConnectedCallback = std::function<void(const std::string& node_id)>;
    using NodeDisconnectedCallback = std::function<void(const std::string& node_id)>;
    using NodeErrorCallback = std::function<void(const std::string& node_id, NetworkError, const std::string&)>;

    // 비동기 요청 결과
    struct AsyncRequestResult {
        uint64_t request_id;
        std::future<protocol::coordinator_node::NetworkMessage> future;
    };

    class NodeTcpClient 
    {
    private:
        std::atomic<bool> is_initialized{false};
        
        NodeConnectionInfo connection_info;
        mutable std::mutex client_mutex;
        
        std::atomic<uint64_t> last_used_time{0};
        std::atomic<bool> is_connected{false};
        
        NodeConnectedCallback connected_callback;
        NodeDisconnectedCallback disconnected_callback;
        NodeErrorCallback error_callback;

        // TLS 관련
        std::unique_ptr<TlsContext> tls_context;
        std::unique_ptr<TlsConnection> tls_connection;

        // Send Queue: 여러 Handler가 요청을 큐잉
        std::unique_ptr<utils::ThreadSafeQueue<protocol::coordinator_node::NetworkMessage>> send_queue;
        
        // Pending Requests: request_id → promise 매핑
        std::unordered_map<uint64_t, std::promise<protocol::coordinator_node::NetworkMessage>> pending_requests;
        std::mutex pending_mutex;
        
        // Request ID 생성기
        std::atomic<uint64_t> next_request_id{1};
        
        // Worker Threads
        std::thread send_thread;
        std::thread receive_thread;
        std::atomic<bool> threads_running{false};

    public:
        NodeTcpClient(const std::string& node_id, 
            const std::string& address, 
            uint16_t port,
            PlatformType platform,
            uint32_t shard_index,
            const std::string& certificate_path,
            const std::string& private_key_id);
        ~NodeTcpClient();

        bool Initialize();
        bool IsInitialized() const { return is_initialized.load(); }

        bool Connect();
        void Disconnect();
        bool IsConnected() const;
        bool EnsureConnection();

        bool SendMessage(const NetworkMessage& message);
        bool ReceiveMessage(NetworkMessage& message);

        AsyncRequestResult SendRequestAsync(const protocol::coordinator_node::BaseRequest* request);
        std::unique_ptr<BaseResponse> SendRequest(const BaseRequest* request);

        void SetConnectedCallback(NodeConnectedCallback callback);
        void SetDisconnectedCallback(NodeDisconnectedCallback callback);
        void SetErrorCallback(NodeErrorCallback callback);

        const NodeConnectionInfo& GetConnectionInfo() const;
        std::string GetNodeId() const { return connection_info.node_id; }
        std::string GetAddress() const { return connection_info.node_address; }
        uint16_t GetPort() const { return connection_info.node_port; }
        PlatformType GetPlatform() const { return connection_info.platform; }
        uint32_t GetShardIndex() const { return connection_info.shard_index; }
        std::string GetEndpoint() const { return connection_info.GetEndpoint(); }
        ConnectionStatus GetStatus() const { return connection_info.status; }

        std::string ToString() const { return connection_info.ToString(); }
        bool IsValid() const { return connection_info.IsValid(); }

    private:
        bool InitializeTlsContext();
        bool InitializeSocket();
        bool ConnectSocket();
        bool EstablishTlsConnection();
        void CleanupSocket();

        void SendLoop();
        void ReceiveLoop();

        bool SendRaw(const void* data, size_t length);
        bool ReceiveRaw(void* buffer, size_t length);

        void NotifyError(NetworkError error, const std::string& message);
        void UpdateConnectionStats(bool success);

        NetworkMessage ConvertToNetworkMessage(const BaseRequest* request);
        std::unique_ptr<BaseResponse> ConvertFromNetworkMessage(const NetworkMessage& message);
    };
}