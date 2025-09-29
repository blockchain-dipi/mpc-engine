// src/node/network/src/NodeTcpServer.cpp
#include "node/network/include/NodeTcpServer.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

namespace mpc_engine::node::network
{
    NodeTcpServer::NodeTcpServer(const std::string& address, uint16_t port) 
        : bind_address(address), bind_port(port) {}

    NodeTcpServer::~NodeTcpServer() 
    {
        Stop();
    }

    bool NodeTcpServer::Initialize() 
    {
        if (is_initialized.load()) {
            return true;
        }

        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET_VALUE) {
            return false;
        }

        SetSocketOptions(server_socket);

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(bind_port);
        
        if (inet_pton(AF_INET, bind_address.c_str(), &server_addr.sin_addr) <= 0) {
            utils::CloseSocket(server_socket);
            return false;
        }

        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            utils::CloseSocket(server_socket);
            return false;
        }

        is_initialized = true;
        return true;
    }

    bool NodeTcpServer::Start() 
    {
        if (!is_initialized.load() || is_running.load()) {
            return false;
        }

        if (listen(server_socket, 1) < 0) {
            return false;
        }

        is_running = true;
        connection_thread = std::thread(&NodeTcpServer::ConnectionLoop, this);
        
        return true;
    }

    void NodeTcpServer::Stop() 
    {
        if (!is_running.load()) {
            return;
        }

        is_running = false;

        if (server_socket != INVALID_SOCKET_VALUE) {
            utils::CloseSocket(server_socket);
            server_socket = INVALID_SOCKET_VALUE;
        }

        ForceCloseExistingConnection();

        if (connection_thread.joinable()) {
            connection_thread.join();
        }
    }

    void NodeTcpServer::ConnectionLoop() {
        std::cout << "Node listening on " << bind_address << ":" << bind_port << std::endl;
        std::cout << "Trusted Coordinator: " << security_config.trusted_coordinator_ip << std::endl;
        
        while (is_running.load()) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            socket_t client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);
            
            if (!is_running.load()) break;
            if (client_socket == INVALID_SOCKET_VALUE) continue;

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            uint16_t client_port = ntohs(client_addr.sin_port);

            // 보안 검증
            if (!IsAuthorized(client_ip)) {
                std::cerr << "[SECURITY] Rejected: " << client_ip << ":" << client_port << std::endl;
                utils::CloseSocket(client_socket);
                continue;
            }

            // 기존 연결 종료
            ForceCloseExistingConnection();

            std::cout << "[SECURITY] Accepted: " << client_ip << ":" << client_port << std::endl;
            HandleCoordinatorConnection(client_socket, client_ip, client_port);
        }
    }

    bool NodeTcpServer::IsAuthorized(const std::string& client_ip) {
        return security_config.IsAllowed(client_ip);
    }

    void NodeTcpServer::ForceCloseExistingConnection() {
        std::lock_guard<std::mutex> lock(connection_mutex);
        
        if (coordinator_connection) {
            std::cout << "Closing existing connection" << std::endl;
            
            if (coordinator_connection->coordinator_socket != INVALID_SOCKET_VALUE) {
                utils::CloseSocket(coordinator_connection->coordinator_socket);
            }
            
            coordinator_connection.reset();
        }
    }

    void NodeTcpServer::HandleCoordinatorConnection(socket_t client_socket, 
                                                    const std::string& client_ip, 
                                                    uint16_t client_port) {
        // 연결 정보 생성
        {
            std::lock_guard<std::mutex> lock(connection_mutex);
            coordinator_connection = std::make_unique<NodeConnectionInfo>();
            coordinator_connection->Initialize(client_socket, client_ip, client_port);
        }

        if (connected_handler) {
            connected_handler(*coordinator_connection);
        }

        // 메시지 처리 루프
        while (is_running.load() && HasActiveConnection()) {
            protocol::coordinator_node::NetworkMessage request_message;
            
            if (!ReceiveMessage(client_socket, request_message)) {
                std::cout << "Connection lost or timeout" << std::endl;
                break;
            }

            coordinator_connection->last_activity_time = utils::GetCurrentTimeMs();
            coordinator_connection->total_requests_handled++;

            if (message_handler) {
                protocol::coordinator_node::NetworkMessage response = message_handler(request_message);
                
                if (SendMessage(client_socket, response)) {
                    coordinator_connection->successful_requests++;
                } else {
                    std::cerr << "Failed to send response" << std::endl;
                    break;
                }
            }
        }

        // 연결 종료
        std::lock_guard<std::mutex> lock(connection_mutex);
        if (coordinator_connection) {
            coordinator_connection->status = ConnectionStatus::DISCONNECTED;
            
            if (disconnected_handler) {
                disconnected_handler(*coordinator_connection);
            }
            
            utils::CloseSocket(coordinator_connection->coordinator_socket);
            coordinator_connection.reset();
        }
    }

    void NodeTcpServer::SetSocketOptions(socket_t sock) {
        utils::SetSocketReuseAddr(sock);
        utils::SetSocketNoDelay(sock);
        
        utils::KeepAliveConfig keepalive;
        keepalive.enabled = true;
        keepalive.idle_seconds = 10;
        keepalive.interval_seconds = 5;
        keepalive.probe_count = 3;
        utils::SetSocketKeepAlive(sock, keepalive);
        
        utils::SetSocketRecvTimeout(sock, 30000);
        utils::SetSocketBufferSize(sock, 64 * 1024, 64 * 1024);
    }

    bool NodeTcpServer::SendMessage(socket_t sock, const protocol::coordinator_node::NetworkMessage& message) {
        if (send(sock, &message.header, sizeof(protocol::coordinator_node::MessageHeader), MSG_NOSIGNAL) <= 0) {
            return false;
        }

        if (message.header.body_length > 0) {
            if (send(sock, message.body.data(), message.body.size(), MSG_NOSIGNAL) <= 0) {
                return false;
            }
        }

        return true;
    }

    bool NodeTcpServer::ReceiveMessage(socket_t sock, protocol::coordinator_node::NetworkMessage& message) {
        ssize_t received = recv(sock, &message.header, sizeof(protocol::coordinator_node::MessageHeader), MSG_WAITALL);
        if (received != sizeof(protocol::coordinator_node::MessageHeader)) {
            return false;
        }

        if (!message.header.IsValid()) {
            std::cerr << "Invalid message header" << std::endl;
            return false;
        }

        if (message.header.body_length > 0) {
            if (message.header.body_length > MAX_MESSAGE_SIZE) {
                std::cerr << "Message too large" << std::endl;
                return false;
            }
            
            message.body.resize(message.header.body_length);
            received = recv(sock, message.body.data(), message.header.body_length, MSG_WAITALL);
            if (received != static_cast<ssize_t>(message.header.body_length)) {
                return false;
            }
        }

        return true;
    }

    void NodeTcpServer::SetTrustedCoordinator(const std::string& ip) {
        security_config.trusted_coordinator_ip = ip;
        std::cout << "[SECURITY] Trusted Coordinator: " << ip << std::endl;
    }

    bool NodeTcpServer::HasActiveConnection() const {
        std::lock_guard<std::mutex> lock(connection_mutex);
        return coordinator_connection && coordinator_connection->IsActive();
    }

    void NodeTcpServer::SetMessageHandler(MessageHandler handler) {
        message_handler = handler;
    }

    void NodeTcpServer::SetConnectedHandler(ConnectionHandler handler) {
        connected_handler = handler;
    }

    void NodeTcpServer::SetDisconnectedHandler(ConnectionHandler handler) {
        disconnected_handler = handler;
    }
}