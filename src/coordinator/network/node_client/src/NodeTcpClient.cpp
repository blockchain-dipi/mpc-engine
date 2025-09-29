#include "coordinator/network/node_client/include/NodeTcpClient.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace mpc_engine::coordinator::network
{
    NodeTcpClient::NodeTcpClient(const std::string& node_id, 
                                 const std::string& address, 
                                 uint16_t port,
                                 NodePlatformType platform,
                                 uint32_t shard_index) {
        connection_info.node_id = node_id;
        connection_info.node_address = address;
        connection_info.node_port = port;
        connection_info.platform = platform;
        connection_info.shard_index = shard_index;
        connection_info.status = ConnectionStatus::DISCONNECTED;
    }

    NodeTcpClient::~NodeTcpClient() {
        Disconnect();
    }

    bool NodeTcpClient::Connect() {
        std::lock_guard<std::mutex> lock(client_mutex);
        
        if (connection_info.status == ConnectionStatus::CONNECTED) {
            return true;
        }

        connection_info.connection_attempt_time = utils::GetCurrentTimeMs();

        if (!InitializeSocket()) {
            connection_info.failed_attempts++;
            return false;
        }

        if (!ConnectSocket()) {
            CleanupSocket();
            connection_info.failed_attempts++;
            return false;
        }

        connection_info.status = ConnectionStatus::CONNECTED;
        connection_info.last_successful_communication = utils::GetCurrentTimeMs();
        connection_info.failed_attempts = 0;
        last_used_time = utils::GetCurrentTimeMs();

        if (connected_callback) {
            connected_callback(connection_info);
        }

        return true;
    }

    void NodeTcpClient::Disconnect() {
        std::lock_guard<std::mutex> lock(client_mutex);
        
        if (connection_info.status == ConnectionStatus::DISCONNECTED) {
            return;
        }

        CleanupSocket();
        connection_info.status = ConnectionStatus::DISCONNECTED;

        if (disconnected_callback) {
            disconnected_callback(connection_info);
        }
    }

    bool NodeTcpClient::IsConnected() const {
        return connection_info.status == ConnectionStatus::CONNECTED;
    }

    bool NodeTcpClient::EnsureConnection() {
        if (IsConnected()) {
            return true;
        }
        return Connect();
    }

    bool NodeTcpClient::SendMessage(const NetworkMessage& message) {
        std::lock_guard<std::mutex> lock(client_mutex);

        if (!IsConnected()) {
            NotifyError(NetworkError::CONNECTION_ERROR, "Not connected");
            return false;
        }

        connection_info.total_requests_sent++;

        if (!SendRaw(&message.header, sizeof(message.header))) {
            NotifyError(NetworkError::SEND_ERROR, "Failed to send header");
            connection_info.failed_responses++;
            return false;
        }

        if (message.header.body_length > 0 && !message.body.empty()) {
            if (!SendRaw(message.body.data(), message.header.body_length)) {
                NotifyError(NetworkError::SEND_ERROR, "Failed to send body");
                connection_info.failed_responses++;
                return false;
            }
        }

        connection_info.last_successful_communication = utils::GetCurrentTimeMs();
        last_used_time = utils::GetCurrentTimeMs();
        return true;
    }

    bool NodeTcpClient::ReceiveMessage(NetworkMessage& message) {
        std::lock_guard<std::mutex> lock(client_mutex);

        if (!IsConnected()) {
            return false;
        }

        if (!ReceiveRaw(&message.header, sizeof(message.header))) {
            connection_info.failed_responses++;
            return false;
        }

        if (message.header.body_length > 0) {
            message.body.resize(message.header.body_length);
            if (!ReceiveRaw(message.body.data(), message.header.body_length)) {
                connection_info.failed_responses++;
                return false;
            }
        }

        connection_info.successful_responses++;
        connection_info.last_successful_communication = utils::GetCurrentTimeMs();
        return true;
    }

    std::unique_ptr<BaseResponse> NodeTcpClient::SendRequest(const BaseRequest* request) {
        if (!request) {
            return nullptr;
        }

        if (!EnsureConnection()) {
            return nullptr;
        }

        NetworkMessage req_msg = ConvertToNetworkMessage(request);
        
        if (!SendMessage(req_msg)) {
            return nullptr;
        }

        NetworkMessage resp_msg;
        if (!ReceiveMessage(resp_msg)) {
            return nullptr;
        }

        return ConvertFromNetworkMessage(resp_msg);
    }

    void NodeTcpClient::SetConnectedCallback(NodeConnectedCallback callback) {
        connected_callback = callback;
    }

    void NodeTcpClient::SetDisconnectedCallback(NodeDisconnectedCallback callback) {
        disconnected_callback = callback;
    }

    void NodeTcpClient::SetErrorCallback(NodeErrorCallback callback) {
        error_callback = callback;
    }

    const NodeConnectionInfo& NodeTcpClient::GetConnectionInfo() const {
        return connection_info;
    }

    bool NodeTcpClient::InitializeSocket() {
        connection_info.node_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (connection_info.node_socket == INVALID_SOCKET_VALUE) {
            NotifyError(NetworkError::SOCKET_CREATE_ERROR, "Failed to create socket");
            return false;
        }

        int opt = 1;
        setsockopt(connection_info.node_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        utils::SetSocketRecvTimeout(connection_info.node_socket, connection_info.connection_timeout_ms);
        utils::SetSocketSendTimeout(connection_info.node_socket, connection_info.connection_timeout_ms);

        return true;
    }

    bool NodeTcpClient::ConnectSocket() {
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(connection_info.node_port);

        if (inet_pton(AF_INET, connection_info.node_address.c_str(), &server_addr.sin_addr) <= 0) {
            NotifyError(NetworkError::INVALID_ADDRESS, "Invalid address");
            return false;
        }

        if (connect(connection_info.node_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            NotifyError(NetworkError::CONNECTION_ERROR, "Connection failed");
            return false;
        }

        return true;
    }

    void NodeTcpClient::CleanupSocket() {
        if (connection_info.node_socket != INVALID_SOCKET_VALUE) {
            close(connection_info.node_socket);
            connection_info.node_socket = INVALID_SOCKET_VALUE;
        }
    }

    bool NodeTcpClient::SendRaw(const void* data, size_t length) {
        size_t total_sent = 0;
        const char* buffer = static_cast<const char*>(data);

        while (total_sent < length) {
            ssize_t sent = send(connection_info.node_socket, 
                              buffer + total_sent, 
                              length - total_sent, 
                              MSG_NOSIGNAL);
            
            if (sent <= 0) {
                return false;
            }
            
            total_sent += sent;
        }

        return true;
    }

    bool NodeTcpClient::ReceiveRaw(void* buffer, size_t length) {
        size_t total_received = 0;
        char* data = static_cast<char*>(buffer);

        while (total_received < length) {
            ssize_t received = recv(connection_info.node_socket, 
                                   data + total_received, 
                                   length - total_received, 
                                   0);
            
            if (received <= 0) {
                return false;
            }
            
            total_received += received;
        }

        return true;
    }

    void NodeTcpClient::NotifyError(NetworkError error, const std::string& message) {
        if (error_callback) {
            error_callback(connection_info, error, message);
        }
    }

    void NodeTcpClient::UpdateConnectionStats(bool success) {
        if (success) {
            connection_info.successful_responses++;
        } else {
            connection_info.failed_responses++;
        }
    }

    NetworkMessage NodeTcpClient::ConvertToNetworkMessage(const BaseRequest* request) {
        return NetworkMessage(static_cast<uint16_t>(request->messageType), "");
    }

    std::unique_ptr<BaseResponse> NodeTcpClient::ConvertFromNetworkMessage(const NetworkMessage& message) {
        auto response = std::make_unique<BaseResponse>(
            static_cast<MessageType>(message.header.message_type)
        );
        response->success = true;
        return response;
    }
}