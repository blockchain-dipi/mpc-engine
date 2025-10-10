// src/node/NodeServer.cpp
#include "node/NodeServer.hpp"
#include "common/config/EnvManager.hpp"
#include "node/handlers/include/NodeProtocolRouter.hpp"
#include "protocols/coordinator_node/include/SigningProtocol.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>

namespace mpc_engine::node
{
    using namespace mpc_engine::env;

    bool NodeConfig::IsValid() const {
        return !node_id.empty() && platform_type != PlatformType::UNKNOWN &&
               bind_port > 0 && !bind_address.empty();
    }

    NodeServer::NodeServer(const NodeConfig& config) {
        if (!config.IsValid()) {
            std::cerr << "Invalid node configuration provided" << std::endl;
            return;
        }
        node_config = config;

        std::cout << "Node configuration set: " << config.node_id 
                  << " (" << PlatformTypeToString(config.platform_type) << ")" << std::endl;

        start_time = utils::GetCurrentTimeMs();
    }

    NodeServer::~NodeServer() {
        Stop();
    }

    bool NodeServer::Initialize() {
        if (is_initialized.load()) {
            return true;
        }

        // Protocol Router 초기화
        if (!handlers::NodeProtocolRouter::Instance().Initialize()) {
            std::cerr << "Failed to initialize node protocol router" << std::endl;
            return false;
        }

        uint16_t handler_threads = Config::GetUInt16("NODE_HANDLER_THREADS");
        tcp_server = std::make_unique<network::NodeTcpServer>(node_config.bind_address, node_config.bind_port, handler_threads);

        if (!tcp_server->Initialize(node_config.certificate_path, node_config.private_key_id)) {
            return false;
        }

        SetupCallbacks();
        is_initialized = true;
        
        std::cout << "Node server initialized with protocol router" << std::endl;
        return true;
    }

    bool NodeServer::Start() {
        if (!is_initialized.load() || is_running.load()) {
            return false;
        }

        if (!tcp_server->Start()) {
            return false;
        }

        is_running = true;
        std::cout << "Node server started: " << node_config.node_id 
                  << " on " << node_config.bind_address << ":" << node_config.bind_port << std::endl;
        return true;
    }

    void NodeServer::Stop() {
        if (!is_running.load()) {
            return;
        }

        if (tcp_server) {
            std::cout << "\nInitiating graceful shutdown..." << std::endl;
            tcp_server->PrepareShutdown(30000);  // 30초 타임아웃
        }

        is_running = false;

        if (tcp_server) {
            tcp_server->Stop();
        }
        
        std::cout << "Node server stopped: " << node_config.node_id << std::endl;
    }

    bool NodeServer::IsRunning() const {
        return is_running.load();
    }

    std::string NodeServer::GetNodeId() const {
        return node_config.node_id;
    }

    PlatformType NodeServer::GetPlatformType() const {
        return node_config.platform_type;
    }

    NodeStats NodeServer::GetStats() const {
        NodeStats stats;
        stats.node_id = node_config.node_id;
        stats.platform_type = node_config.platform_type;
        stats.status = is_running.load() ? ConnectionStatus::CONNECTED : ConnectionStatus::DISCONNECTED;
        stats.active_connections = tcp_server && tcp_server->HasActiveConnection() ? 1 : 0;
        stats.uptime_seconds = (utils::GetCurrentTimeMs() - start_time) / 1000;
        
        // TCP 서버에서 추가 통계 수집 (향후 구현)
        // stats.total_requests = tcp_server->GetTotalRequests();
        // stats.successful_requests = tcp_server->GetSuccessfulRequests();
        
        return stats;
    }

    void NodeServer::OnCoordinatorConnected(const network::NodeConnectionInfo& connection) {
        std::cout << "Coordinator connected to node " << node_config.node_id 
                  << ": " << connection.ToString() << std::endl;
    }

    void NodeServer::OnCoordinatorDisconnected(const network::NodeConnectionInfo::DisconnectionInfo& connection) {
        std::cout << "Coordinator disconnected from node " << node_config.node_id 
                  << ": " << connection.coordinator_address << std::endl;
    }

    NetworkMessage NodeServer::ProcessMessage(const NetworkMessage& message) {
        std::cout << "Node " << node_config.node_id << " processing message type: " 
                  << message.header.message_type << std::endl;

        try {
            // 메시지를 BaseRequest로 변환 (간단한 구현)
            auto request = ConvertToBaseRequest(message);
            if (!request) {
                std::cerr << "Failed to convert network message to base request" << std::endl;
                return CreateErrorResponse(message.header.message_type, "Invalid message format");
            }

            // Protocol Router를 통해 처리
            auto response = handlers::NodeProtocolRouter::Instance().ProcessMessage(
                static_cast<MessageType>(message.header.message_type), 
                request.get()
            );

            if (!response) {
                std::cerr << "No response from protocol router" << std::endl;
                return CreateErrorResponse(message.header.message_type, "No response generated");
            }

            // 응답을 NetworkMessage로 변환
            return ConvertToNetworkMessage(*response);

        } catch (const std::exception& e) {
            std::cerr << "Exception in ProcessMessage: " << e.what() << std::endl;
            return CreateErrorResponse(message.header.message_type, "Processing failed: " + std::string(e.what()));
        }
    }

    void NodeServer::SetupCallbacks() {
        if (!tcp_server) {
            return;
        }

        tcp_server->SetConnectedHandler([this](const network::NodeConnectionInfo& info) {
            OnCoordinatorConnected(info);
        });

        tcp_server->SetDisconnectedHandler([this](const network::NodeConnectionInfo::DisconnectionInfo& info) {
            OnCoordinatorDisconnected(info);
        });

        tcp_server->SetMessageHandler([this](const NetworkMessage& msg) {
            return ProcessMessage(msg);
        });
        
        std::cout << "Node server callbacks configured" << std::endl;
    }

    std::unique_ptr<BaseRequest> NodeServer::ConvertToBaseRequest(const NetworkMessage& message) {
        using namespace protocol::coordinator_node;
        
        MessageType msgType = static_cast<MessageType>(message.header.message_type);
        
        switch (msgType) {
            case MessageType::SIGNING_REQUEST: {
                auto request = std::make_unique<SigningRequest>();
                
                // 간단한 페이로드 파싱 (실제로는 더 정교한 직렬화/역직렬화 필요)
                std::string payload = message.GetBodyAsString();
                
                // 기본값 설정 또는 페이로드에서 파싱
                if (!payload.empty()) {
                    // 임시로 간단한 파싱 ("|"로 구분된 형태)
                    size_t pos1 = payload.find('|');
                    if (pos1 != std::string::npos) {
                        request->uid = payload.substr(0, pos1);
                        size_t pos2 = payload.find('|', pos1 + 1);
                        if (pos2 != std::string::npos) {
                            request->sendTime = payload.substr(pos1 + 1, pos2 - pos1 - 1);
                        }
                    }
                }
                
                // 기본 서명 요청 데이터 설정
                request->keyId = "default_key_" + node_config.node_id;
                request->transactionData = "0x" + std::string(64, '0');  // 빈 트랜잭션 데이터
                request->threshold = 2;
                request->totalShards = 3;
                
                return request;
            }
            
            default:
                // 다른 메시지 타입들은 향후 구현
                return std::make_unique<BaseRequest>(msgType);
        }
    }

    NetworkMessage NodeServer::ConvertToNetworkMessage(const BaseResponse& response) {
        // 응답을 간단한 페이로드로 변환
        std::string payload = "success=" + std::string(response.success ? "true" : "false");
        
        if (!response.errorMessage.empty()) {
            payload += "|error=" + response.errorMessage;
        }
        
        // SigningResponse 특별 처리
        if (response.messageType == MessageType::SIGNING_REQUEST) {
            const SigningResponse* signingResponse = 
                dynamic_cast<const SigningResponse*>(&response);
            if (signingResponse && signingResponse->success) {
                payload += "|signature=" + signingResponse->signature;
                payload += "|keyId=" + signingResponse->keyId;
                payload += "|shardIndex=" + std::to_string(signingResponse->shardIndex);
            }
        }
        
        return NetworkMessage(
            static_cast<uint16_t>(response.messageType), 
            payload
        );
    }

    NetworkMessage NodeServer::CreateErrorResponse(uint16_t messageType, const std::string& errorMessage) {
        std::string payload = "success=false|error=" + errorMessage;
        return NetworkMessage(messageType, payload);
    }
}