// src/node/NodeServer.cpp
#include "node/NodeServer.hpp"
#include "common/config/EnvManager.hpp"
#include "node/handlers/include/NodeMessageRouter.hpp"
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

        // Message Router 초기화
        if (!handlers::NodeMessageRouter::Instance().Initialize()) {
            std::cerr << "Failed to initialize node message router" << std::endl;
            return false;
        }

        uint16_t handler_threads = Config::GetUInt16("NODE_HANDLER_THREADS");
        tcp_server = std::make_unique<network::NodeTcpServer>(node_config.bind_address, node_config.bind_port, handler_threads);

        if (!tcp_server->Initialize(node_config.certificate_path, node_config.private_key_id)) {
            return false;
        }

        SetupCallbacks();
        is_initialized = true;

        std::cout << "Node server initialized with message router" << std::endl;
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
            // 1. NetworkMessage → Proto 변환
            std::unique_ptr<CoordinatorNodeMessage> proto_request = NetworkMessageToProto(message);
            if (!proto_request) {
                std::cerr << "Failed to parse protobuf message" << std::endl;
                return CreateErrorResponse(message.header.message_type, "Invalid protobuf format");
            }

            // 2. NodeMessageRouter로 처리 (Proto 메시지 사용)
            std::unique_ptr<CoordinatorNodeMessage> proto_response = 
                handlers::NodeMessageRouter::Instance().ProcessMessage(proto_request.get());

            if (!proto_response) {
                std::cerr << "No response from message router" << std::endl;
                return CreateErrorResponse(message.header.message_type, "No response generated");
            }

            // 3. Proto → NetworkMessage 변환
            return ProtoToNetworkMessage(proto_response.get());

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

    // NetworkMessage → Proto 변환
    std::unique_ptr<CoordinatorNodeMessage> NodeServer::NetworkMessageToProto(const NetworkMessage& message) {
        auto proto_msg = std::make_unique<CoordinatorNodeMessage>();
        
        // body에서 Proto 메시지 역직렬화
        if (!message.body.empty()) {
            if (!proto_msg->ParseFromArray(message.body.data(), message.body.size())) {
                std::cerr << "[NodeServer] Failed to parse protobuf from NetworkMessage" << std::endl;
                return nullptr;
            }
        }
        
        return proto_msg;
    }

    // Proto → NetworkMessage 변환
    NetworkMessage NodeServer::ProtoToNetworkMessage(const CoordinatorNodeMessage* proto_msg) {
        if (!proto_msg) {
            throw std::invalid_argument("Proto message is null");
        }
        
        // Proto 메시지 직렬화
        std::string serialized;
        if (!proto_msg->SerializeToString(&serialized)) {
            throw std::runtime_error("Failed to serialize protobuf message");
        }
        
        // NetworkMessage 생성
        NetworkMessage network_msg;
        network_msg.header.message_type = static_cast<uint16_t>(proto_msg->message_type());
        network_msg.header.body_length = static_cast<uint32_t>(serialized.size());
        network_msg.body.assign(serialized.begin(), serialized.end());
        
        // Checksum 계산
        network_msg.header.checksum = MessageHeader::ComputeChecksum(network_msg.body);
        
        return network_msg;
    }

    NetworkMessage NodeServer::CreateErrorResponse(uint16_t messageType, const std::string& errorMessage) {
        std::string payload = "success=false|error=" + errorMessage;
        return NetworkMessage(messageType, payload);
    }
}