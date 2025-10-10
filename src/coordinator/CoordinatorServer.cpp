// src/coordinator/CoordinatorServer.cpp
#include "coordinator/CoordinatorServer.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/kms/include/KMSManager.hpp"
#include "common/config/EnvManager.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>

namespace mpc_engine::coordinator
{
    using namespace mpc_engine::network::tls;
    using namespace mpc_engine::env;
    using namespace mpc_engine::kms;

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

    bool CoordinatorServer::InitializeWalletServer(
        const std::string& wallet_url,
        const std::string& auth_token
    ) {
        std::cout << "[CoordinatorServer] Initializing Wallet Server..." << std::endl;

        // TLS Context 생성 (Wallet 통신용)
        auto& kms = KMSManager::Instance();

        // CA 인증서 로드
        std::string tls_ca = EnvManager::Instance().GetString("TLS_KMS_CA_KEY_ID");
        std::string ca_pem = kms.GetSecret(tls_ca);
        if (ca_pem.empty()) {
            std::cerr << "[CoordinatorServer] Failed to load CA certificate from KMS" << std::endl;
            return false;
        }

        // Coordinator-Wallet 서버 인증서 로드
        std::string coordinator_wallet_cert_path = EnvManager::Instance().GetString("TLS_DOCKER_COORDINATOR_WALLET");
        std::string coordinator_wallet_key_id = EnvManager::Instance().GetString("TLS_KMS_COORDINATOR_WALLET_KEY_ID");

        std::ifstream cert_file(coordinator_wallet_cert_path);
        if (!cert_file) {
            std::cerr << "[CoordinatorServer] Failed to read coordinator wallet certificate" << std::endl;
            return false;
        }
        std::string certificate_pem((std::istreambuf_iterator<char>(cert_file)), std::istreambuf_iterator<char>());
        std::string private_key_pem = kms.GetSecret(coordinator_wallet_key_id);

        if (certificate_pem.empty() || private_key_pem.empty()) {
            std::cerr << "[CoordinatorServer] Empty coordinator wallet certificate or key" << std::endl;
            return false;
        }

        TlsContext tls_ctx;
        TlsConfig config = TlsConfig::CreateSecureServerConfig(); // 서버 모드

        if (!tls_ctx.Initialize(config)) {
            std::cerr << "[CoordinatorServer] Failed to initialize TLS context" << std::endl;
            return false;
        }

        if (!tls_ctx.LoadCA(ca_pem)) {
            std::cerr << "[CoordinatorServer] Failed to load CA certificate" << std::endl;
            return false;
        }

        CertificateData cert_data;
        cert_data.certificate_pem = certificate_pem;
        cert_data.private_key_pem = private_key_pem;

        if (!tls_ctx.LoadCertificate(cert_data)) {
            std::cerr << "[CoordinatorServer] Failed to load coordinator wallet certificate" << std::endl;
            return false;
        }

        if (!wallet_manager_.Initialize(wallet_url, auth_token, tls_ctx)) {
            std::cerr << "[CoordinatorServer] Failed to initialize Wallet Server" << std::endl;
            return false;
        }

        std::cout << "[CoordinatorServer] Wallet Server initialized successfully" << std::endl;
        return true;
    }

    std::unique_ptr<protocol::coordinator_wallet::WalletSigningResponse> 
    CoordinatorServer::SendToWallet(
        const protocol::coordinator_wallet::WalletSigningRequest& request
    ) {
        if (!wallet_manager_.IsInitialized()) {
            std::cerr << "[CoordinatorServer] Wallet Server not initialized" << std::endl;
            return nullptr;
        }

        std::cout << "[CoordinatorServer] Sending request to Wallet Server..." << std::endl;
        return wallet_manager_.SendSigningRequest(request);
    }

    bool CoordinatorServer::IsWalletServerInitialized() const 
    {
        return wallet_manager_.IsInitialized();
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
        PlatformType platform,
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

        // Node별 인증서 정보 찾기
        std::vector<std::string> node_ids = Config::GetStringArray("NODE_IDS");
        std::vector<std::string> tls_cert_paths = Config::GetStringArray("TLS_CERT_PATHS");
        std::vector<std::string> tls_kms_nodes_key_ids = Config::GetStringArray("TLS_KMS_NODES_KEY_IDS");

        std::string certificate_path;
        std::string private_key_id;
        
        bool found = false;
        for (size_t i = 0; i < node_ids.size(); ++i) {
            if (node_ids[i] == node_id) {
                if (i < tls_cert_paths.size() && i < tls_kms_nodes_key_ids.size()) {
                    certificate_path = tls_cert_paths[i];
                    private_key_id = tls_kms_nodes_key_ids[i];
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            std::cerr << "Certificate configuration not found for node: " << node_id << std::endl;
            return false;
        }

        std::unique_ptr<network::NodeTcpClient> node_client 
        = std::make_unique<network::NodeTcpClient>(node_id, address, port, platform, shard_index, certificate_path, private_key_id);
        
        node_client->SetConnectedCallback([this](const std::string& node_id) 
        {
            OnNodeStatusChanged(node_id, ConnectionStatus::CONNECTED);
        });
        
        node_client->SetDisconnectedCallback([this](const std::string& node_id) 
        {
            OnNodeStatusChanged(node_id, ConnectionStatus::DISCONNECTED);
        });
        
        node_clients[node_id] = std::move(node_client);
        
        std::cout << "Node registered: " << node_id << " at " << address << ":" << port 
                  << " (platform: " << PlatformTypeToString(platform) 
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
        if (node_ids.empty()) {
            return true;
        }
    
        // 1. 모든 Node에 비동기 요청
        std::vector<std::pair<std::string, std::future<std::unique_ptr<BaseResponse>>>> futures;
        
        for (const std::string& node_id : node_ids) 
        {
            // SendToNode()를 비동기로 실행
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
                // 타임아웃 35초 (SendRequest의 30초 + 여유 5초)
                if (pair.second.wait_for(std::chrono::seconds(35)) == std::future_status::timeout) {
                    std::cerr << "Broadcast timeout for node: " << node_id << std::endl;
                    all_success = false;
                    continue;
                }
                
                std::unique_ptr<BaseResponse> response = pair.second.get();
                
                if (!response || !response->success) {
                    std::cerr << "Broadcast failed for node: " << node_id << std::endl;
                    all_success = false;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Broadcast exception for node: " << node_id 
                          << ", error: " << e.what() << std::endl;
                all_success = false;
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

    std::vector<std::string> CoordinatorServer::GetNodesByPlatform(PlatformType platform) const 
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