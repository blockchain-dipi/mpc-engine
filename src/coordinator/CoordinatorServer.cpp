// src/coordinator/CoordinatorServer.cpp
#include "coordinator/CoordinatorServer.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/env/EnvManager.hpp"
#include "common/utils/logger/Logger.hpp"
#include <future>

namespace mpc_engine::coordinator
{
    using namespace mpc_engine::network::tls;
    using namespace mpc_engine::env;

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
        LOG_INFO("CoordinatorServer", "Coordinator server started");
        return true;
    }

    void CoordinatorServer::Stop() 
    {
        if (!is_running.load()) 
        {
            LOG_WARN("CoordinatorServer", "Coordinator server is not running");
            return;
        }
    
        is_running = false;
        
        // HTTPS 서버 중지
        StopHttpsServer();
        
        // 모든 Node 연결 해제
        DisconnectAllNodes();

        LOG_INFO("CoordinatorServer", "Coordinator server stopped");
    }

    bool CoordinatorServer::IsRunning() const 
    {
        return is_running.load();
    }

    // ========================================
    // Node 관리
    // ========================================

    bool CoordinatorServer::RegisterNode(
        const std::string& node_id,
        PlatformType platform,
        const std::string& address,
        uint16_t port,
        uint32_t shard_index) 
    {
        if (node_id.empty() || address.empty() || port == 0) 
        {
            LOG_ERROR("CoordinatorServer", "Invalid node parameters");
            return false;
        }

        std::lock_guard<std::mutex> lock(nodes_mutex);
        
        if (node_clients.find(node_id) != node_clients.end()) 
        {
            LOG_ERRORF("CoordinatorServer", "Node already registered: %s", node_id.c_str());
            return false;
        }

        // Node별 인증서 정보 찾기
        std::vector<std::string> node_ids = Config::GetStringArray("NODE_IDS");
        std::vector<std::string> tls_cert_paths = Config::GetStringArray("TLS_CERT_PATHS");
        std::vector<std::string> tls_kms_nodes_coordinator_key_ids = Config::GetStringArray("TLS_KMS_NODES_COORDINATOR_KEY_IDS");

        std::string certificate_path;
        std::string private_key_id;
        
        bool found = false;
        for (size_t i = 0; i < node_ids.size(); ++i) {
            if (node_ids[i] == node_id) {
                if (i < tls_cert_paths.size() && i < tls_kms_nodes_coordinator_key_ids.size()) {
                    certificate_path = tls_cert_paths[i];
                    private_key_id = tls_kms_nodes_coordinator_key_ids[i];
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            LOG_ERRORF("CoordinatorServer", "Certificate configuration not found for node: %s", node_id.c_str());
            return false;
        }

        std::unique_ptr<network::NodeTcpClient> node_client = std::make_unique<network::NodeTcpClient>(
            node_id, address, port, platform, shard_index, certificate_path, private_key_id
        );
        
        if (!node_client->Initialize()) {
            LOG_ERRORF("CoordinatorServer", "Failed to initialize TLS context for node: %s", node_id.c_str());
            return false;
        }
        
        node_client->SetConnectedCallback([this](const std::string& node_id) 
        {
            OnNodeStatusChanged(node_id, ConnectionStatus::CONNECTED);
        });
        
        node_client->SetDisconnectedCallback([this](const std::string& node_id) 
        {
            OnNodeStatusChanged(node_id, ConnectionStatus::DISCONNECTED);
        });
        
        node_clients[node_id] = std::move(node_client);

        LOG_INFOF("CoordinatorServer", "Node registered: %s at %s:%d (platform: %s, shard: %d)",
                  node_id.c_str(), address.c_str(), port, PlatformTypeToString(platform).c_str(), shard_index);

        return true;
    }

    void CoordinatorServer::UnregisterNode(const std::string& node_id) 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        
        auto it = node_clients.find(node_id);
        if (it != node_clients.end()) 
        {
            it->second->Disconnect();
            node_clients.erase(it);
            LOG_INFOF("CoordinatorServer", "Node unregistered: %s", node_id.c_str());
        }
    }

    bool CoordinatorServer::HasNode(const std::string& node_id) const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        return node_clients.find(node_id) != node_clients.end();
    }

    // ========================================
    // Node 연결
    // ========================================

    bool CoordinatorServer::ConnectToNode(const std::string& node_id) 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        if (!client) {
            LOG_ERRORF("CoordinatorServer", "Node not found: %s", node_id.c_str());
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

        for (auto& entry : node_clients)
        {
            if (entry.second) 
            {
                entry.second->Disconnect();
            }
        }
    }

    // ========================================
    // Node 통신
    // ========================================

    std::unique_ptr<CoordinatorNodeMessage> CoordinatorServer::SendToNode(
        const std::string& node_id, 
        const CoordinatorNodeMessage* request) 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        if (!client) 
        {
            LOG_ERRORF("CoordinatorServer", "Node not found: %s", node_id.c_str());
            return nullptr;
        }

        return client->SendRequest(request);
    }

    bool CoordinatorServer::BroadcastToNodes(
        const std::vector<std::string>& node_ids, 
        const CoordinatorNodeMessage* request) 
    {
        if (node_ids.empty()) {
            LOG_WARN("CoordinatorServer", "No node IDs provided for broadcast");
            return true;
        }
    
        // 1. 모든 Node에 비동기 요청
        std::vector<std::pair<std::string, std::future<std::unique_ptr<CoordinatorNodeMessage>>>> futures;
        
        for (const std::string& node_id : node_ids) 
        {
            auto future = std::async(std::launch::async, [this, node_id, request]() {
                return SendToNode(node_id, request);
            });
            
            futures.emplace_back(node_id, std::move(future));
        }
    
        // 2. 모든 응답 대기 및 결과 확인
        bool all_success = true;
        
        for (auto& pair : futures) 
        {
            const std::string& node_id = pair.first;
            
            try {
                // 타임아웃 35초
                if (pair.second.wait_for(std::chrono::seconds(35)) == std::future_status::timeout) {
                    LOG_ERRORF("CoordinatorServer", "Broadcast timeout for node: %s", node_id.c_str());
                    all_success = false;
                    continue;
                }
                
                std::unique_ptr<CoordinatorNodeMessage> response = pair.second.get();
                
                if (!response) {
                    LOG_ERRORF("CoordinatorServer", "Broadcast failed for node: %s - null response", node_id.c_str());
                    all_success = false;
                    continue;
                }

                if (response->has_signing_response()) {
                    const SigningResponse& signing_resp = response->signing_response();
                    if (!signing_resp.header().success()) {
                        LOG_ERRORF("CoordinatorServer", "Broadcast failed for node: %s - error: %s", 
                            node_id.c_str(), signing_resp.header().error_message().c_str());
                        all_success = false;
                    }
                }
                
            } catch (const std::exception& e) {
                LOG_ERRORF("CoordinatorServer", "Broadcast exception for node: %s, error: %s", node_id.c_str(), e.what());
                all_success = false;
            }
        }
        
        return all_success;
    }

    bool CoordinatorServer::BroadcastToAllConnectedNodes(const CoordinatorNodeMessage* request) 
    {
        std::vector<std::string> connected_nodes = GetConnectedNodeIds();
        return BroadcastToNodes(connected_nodes, request);
    }

    // ========================================
    // Node 상태 조회
    // ========================================

    std::vector<std::string> CoordinatorServer::GetConnectedNodeIds() const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const auto& entry : node_clients) 
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
        return GetConnectedNodeIds();
    }

    std::vector<std::string> CoordinatorServer::GetAllNodeIds() const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const auto& entry : node_clients)
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

    PlatformType CoordinatorServer::GetNodePlatform(const std::string& node_id) const 
    {
        network::NodeTcpClient* client = FindNodeClientInternal(node_id);
        return client ? client->GetPlatform() : PlatformType::UNKNOWN;
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
        
        for (const auto& entry : node_clients)
        {
            if (entry.second && entry.second->IsConnected()) 
            {
                stats.connected_nodes++;
                stats.ready_nodes++;
            }
        }
        
        return stats;
    }

    std::vector<std::string> CoordinatorServer::GetNodesByPlatform(PlatformType platform) const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const auto& entry : node_clients)
        {
            if (entry.second && entry.second->GetPlatform() == platform) 
            {
                result.push_back(entry.first);
            }
        }

        return result;
    }

    std::vector<std::string> CoordinatorServer::GetNodesByStatus(ConnectionStatus status) const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        std::vector<std::string> result;

        for (const auto& entry : node_clients)
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

        for (const auto& entry : node_clients)
        {
            if (entry.second && entry.second->GetShardIndex() == shard_index) 
            {
                result.push_back(entry.first);
            }
        }

        return result;
    }

    // ========================================
    // HTTPS Server 관리
    // ========================================

    bool CoordinatorServer::InitializeHttpsServer() 
    {
        using namespace network::wallet_server;

        LOG_INFO("CoordinatorServer", "Initializing HTTPS server...");

        try {
            // 환경변수에서 설정 로드
            auto& env = EnvManager::Instance();

            HttpsServerConfig config;
            config.bind_address = env.GetString("COORDINATOR_HTTPS_BIND");
            config.bind_port = env.GetUInt16("COORDINATOR_HTTPS_PORT");
            config.max_connections = env.GetUInt32("COORDINATOR_HTTPS_MAX_CONNECTIONS");
            config.handler_threads = env.GetUInt32("COORDINATOR_HTTPS_HANDLER_THREADS");
            
            // TLS 설정 (TLS_CERT_PATH 기준 상대 경로)
            config.tls_cert_path = env.GetString("TLS_CERT_COORDINATOR_WALLET");
            config.tls_key_id = env.GetString("TLS_KMS_COORDINATOR_WALLET_KEY_ID");

            if (config.tls_cert_path.empty() || config.tls_key_id.empty()) {
                LOG_ERROR("CoordinatorServer", "Missing TLS configuration");
                LOG_ERRORF("CoordinatorServer", "  TLS_CERT_COORDINATOR_WALLET: %s", config.tls_cert_path.c_str());
                LOG_ERRORF("CoordinatorServer", "  TLS_KMS_COORDINATOR_WALLET_KEY_ID: %s", config.tls_key_id.c_str());
                return false;
            }

            // HTTPS 서버 생성 및 초기화
            https_server = std::make_unique<CoordinatorHttpsServer>(config);
            
            if (!https_server->Initialize()) {
                LOG_ERROR("CoordinatorServer", "Failed to initialize HTTPS server");
                return false;
            }

            LOG_INFO("CoordinatorServer", "HTTPS server initialized successfully");
            LOG_INFOF("CoordinatorServer", "  Bind: %s:%d", config.bind_address.c_str(), config.bind_port);
            LOG_INFOF("CoordinatorServer", "  Max Connections: %d", config.max_connections);
            LOG_INFOF("CoordinatorServer", "  Handler Threads: %d", config.handler_threads);

            return true;

        } catch (const std::exception& e) {
            LOG_ERRORF("CoordinatorServer", "Failed to initialize HTTPS server: %s", e.what());
            return false;
        }
    }

    bool CoordinatorServer::StartHttpsServer() 
    {
        if (!https_server) {
            LOG_ERROR("CoordinatorServer", "HTTPS server not initialized");
            return false;
        }

        LOG_INFO("CoordinatorServer", "Starting HTTPS server...");

        if (!https_server->Start()) {
            LOG_ERROR("CoordinatorServer", "Failed to start HTTPS server");
            return false;
        }

        LOG_INFO("CoordinatorServer", "HTTPS server started successfully");
        return true;
    }

    void CoordinatorServer::StopHttpsServer() 
    {
        if (https_server) {
            LOG_INFO("CoordinatorServer", "Stopping HTTPS server...");
            https_server->Stop();
            LOG_INFO("CoordinatorServer", "HTTPS server stopped");
        }
    }

    bool CoordinatorServer::IsHttpsServerRunning() const 
    {
        return https_server && https_server->IsRunning();
    }

    // ========================================
    // Private 메서드
    // ========================================

    network::NodeTcpClient* CoordinatorServer::FindNodeClientInternal(const std::string& node_id) const 
    {
        std::lock_guard<std::mutex> lock(nodes_mutex);
        
        auto it = node_clients.find(node_id);
        return (it != node_clients.end()) ? it->second.get() : nullptr;
    }

    void CoordinatorServer::OnNodeStatusChanged(const std::string& node_id, ConnectionStatus status) 
    {
        LOG_INFOF("CoordinatorServer", "Node %s status changed to: %d", node_id.c_str(), static_cast<int>(status));
    }

} // namespace mpc_engine::coordinator