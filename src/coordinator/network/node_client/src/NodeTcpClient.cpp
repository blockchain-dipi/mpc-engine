#include "coordinator/network/node_client/include/NodeTcpClient.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/utils/threading/ThreadUtils.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace mpc_engine::coordinator::network
{
    NodeTcpClient::NodeTcpClient(
        const std::string& node_id, 
        const std::string& address, 
        uint16_t port,
        NodePlatformType platform,
        uint32_t shard_index
    ) {
        connection_info.node_id = node_id;
        connection_info.node_address = address;
        connection_info.node_port = port;
        connection_info.platform = platform;
        connection_info.shard_index = shard_index;
        connection_info.status = ConnectionStatus::DISCONNECTED;

        // Send Queue 초기화
        send_queue = std::make_unique<utils::ThreadSafeQueue<protocol::coordinator_node::NetworkMessage>>(100);
    }

    NodeTcpClient::~NodeTcpClient() {
        Disconnect();
    }

    bool NodeTcpClient::Connect() {
        std::lock_guard<std::mutex> lock(client_mutex);

        if (is_connected.load()) {
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

        is_connected = true;
        threads_running = true;
        
        send_thread = std::thread(&NodeTcpClient::SendLoop, this);
        receive_thread = std::thread(&NodeTcpClient::ReceiveLoop, this);

        std::cout << "[NodeTcpClient] Threads started for " << connection_info.node_id << std::endl;

        if (connected_callback) {
            connected_callback(connection_info.node_id);
        }
        return true;
    }

    void NodeTcpClient::Disconnect() {
        std::string node_id_copy;
        
        {
            std::lock_guard<std::mutex> lock(client_mutex);            
            if (!is_connected.load()) {
                return;
            }

            node_id_copy = connection_info.node_id;  // ← node_id만 복사

            is_connected = false;
            threads_running = false;

            if (send_queue) {
                send_queue->Shutdown();
            }

            CleanupSocket();

            connection_info.status = ConnectionStatus::DISCONNECTED;
        }  // lock 해제

        // 스레드 Join
        constexpr uint32_t THREAD_JOIN_TIMEOUT_MS = 5000;

        if (send_thread.joinable()) {
            utils::JoinResult result = utils::JoinWithTimeout(send_thread, THREAD_JOIN_TIMEOUT_MS);
            if (result == utils::JoinResult::TIMEOUT) {
                std::cerr << "[NodeTcpClient] Send thread join timeout" << std::endl;
            }
        }

        if (receive_thread.joinable()) {
            utils::JoinResult result = utils::JoinWithTimeout(receive_thread, THREAD_JOIN_TIMEOUT_MS);
            if (result == utils::JoinResult::TIMEOUT) {
                std::cerr << "[NodeTcpClient] Receive thread join timeout" << std::endl;
            }
        }

        // Pending Requests 정리
        {
            std::lock_guard<std::mutex> pending_lock(pending_mutex);
            for (auto& pair : pending_requests) {
                try {
                    pair.second.set_exception(
                        std::make_exception_ptr(std::runtime_error("Connection closed"))
                    );
                } catch (...) {}
            }
            pending_requests.clear();
        }

        // Callback 호출 (lock 밖, 간단히)
        if (disconnected_callback) {
            disconnected_callback(node_id_copy);  // ← 로그용, 간단!
        }
    }

    bool NodeTcpClient::IsConnected() const {
        return is_connected.load();
    }

    bool NodeTcpClient::EnsureConnection() {
        if (IsConnected()) {
            return true;
        }
        return Connect();
    }

    void NodeTcpClient::SendLoop() {
        using namespace protocol::coordinator_node;
        
        std::cout << "[NodeTcpClient::SendLoop] Started for " << connection_info.node_id << std::endl;
        
        while (threads_running.load()) {
            NetworkMessage msg;

            // Send Queue에서 Pop
            utils::QueueResult result = send_queue->Pop(msg);

            if (result == utils::QueueResult::SHUTDOWN) {
                std::cout << "[NodeTcpClient::SendLoop] Queue shutdown" << std::endl;
                break;
            }

            if (result != utils::QueueResult::SUCCESS) {
                std::cerr << "[NodeTcpClient::SendLoop] Pop failed: " 
                          << utils::ToString(result) << std::endl;
                break;
            }

            // 메시지 전송
            if (!SendMessage(msg)) {
                std::cerr << "[NodeTcpClient::SendLoop] SendMessage failed" << std::endl;

                // 전송 실패 시 해당 요청의 Promise에 에러 설정
                uint64_t req_id = msg.header.request_id;
                std::lock_guard<std::mutex> lock(pending_mutex);
                auto it = pending_requests.find(req_id);
                if (it != pending_requests.end()) {
                    try {
                        it->second.set_exception(
                            std::make_exception_ptr(std::runtime_error("Send failed"))
                        );
                    } catch (...) {}
                    pending_requests.erase(it);
                }

                break;
            }

            connection_info.last_successful_communication = utils::GetCurrentTimeMs();
        }

        std::cout << "[NodeTcpClient::SendLoop] Stopped" << std::endl;
    }

    void NodeTcpClient::ReceiveLoop() {
        using namespace protocol::coordinator_node;
        
        std::cout << "[NodeTcpClient::ReceiveLoop] Started for " << connection_info.node_id << std::endl;
        
        while (threads_running.load()) {
            NetworkMessage response;

            // 응답 수신
            if (!ReceiveMessage(response)) {
                std::cerr << "[NodeTcpClient::ReceiveLoop] ReceiveMessage failed" << std::endl;
                break;
            }

            uint64_t req_id = response.header.request_id;

            // request_id로 대응하는 Promise 찾기
            std::lock_guard<std::mutex> lock(pending_mutex);
            auto it = pending_requests.find(req_id);

            if (it != pending_requests.end()) {
                // Promise 완료
                try {
                    it->second.set_value(std::move(response));
                } catch (const std::exception& e) {
                    std::cerr << "[NodeTcpClient::ReceiveLoop] set_value failed: " 
                              << e.what() << std::endl;
                }
                pending_requests.erase(it);
            } else {
                std::cerr << "[NodeTcpClient::ReceiveLoop] No pending request for ID: " 
                          << req_id << std::endl;
            }

            connection_info.successful_responses++;
            connection_info.last_successful_communication = utils::GetCurrentTimeMs();
        }

        std::cout << "[NodeTcpClient::ReceiveLoop] Stopped" << std::endl;
    }

    bool NodeTcpClient::SendMessage(const NetworkMessage& message) {
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

    AsyncRequestResult NodeTcpClient::SendRequestAsync(const protocol::coordinator_node::BaseRequest* request) {
        using namespace protocol::coordinator_node;

        if (!request) {
            throw std::invalid_argument("Request is null");
        }

        if (!IsConnected()) {
            throw std::runtime_error("Not connected to node: " + connection_info.node_id);
        }

        // 1. Request ID 생성
        uint64_t req_id = next_request_id.fetch_add(1);

        // 2. Promise 생성 및 등록
        std::promise<NetworkMessage> promise;
        std::future<NetworkMessage> future = promise.get_future();

        {
            std::lock_guard<std::mutex> lock(pending_mutex);
            pending_requests[req_id] = std::move(promise);
        }

        // 3. NetworkMessage 생성
        NetworkMessage msg = ConvertToNetworkMessage(request);
        msg.header.request_id = req_id;
        msg.header.timestamp = utils::GetCurrentTimeMs();

        // 4. Send Queue에 Push
        utils::QueueResult result = send_queue->TryPush(msg, std::chrono::milliseconds(1000));

        if (result != utils::QueueResult::SUCCESS) {
            // Push 실패 시 pending에서 제거
            {
                std::lock_guard<std::mutex> lock(pending_mutex);
                pending_requests.erase(req_id);
            }

            throw std::runtime_error(
                "Failed to push request to queue: " + std::string(utils::ToString(result))
            );
        }

        connection_info.total_requests_sent++;

        // 🆕 request_id와 future를 함께 반환
        return AsyncRequestResult{req_id, std::move(future)};
    }

    std::unique_ptr<BaseResponse> NodeTcpClient::SendRequest(const BaseRequest* request) {
        if (!request) {
            return nullptr;
        }

        if (!EnsureConnection()) {
            return nullptr;
        }

        try {
            // 비동기 요청 (request_id 포함)
            auto result = SendRequestAsync(request);

            // 타임아웃 30초로 대기
            if (result.future.wait_for(std::chrono::seconds(30)) == std::future_status::timeout) {
                std::cerr << "[NodeTcpClient] Request timeout (30s) for node: " 
                          << connection_info.node_id 
                          << ", request_id: " << result.request_id << std::endl;

                // ✅ pending에서 제거
                {
                    std::lock_guard<std::mutex> lock(pending_mutex);
                    auto it = pending_requests.find(result.request_id);
                    if (it != pending_requests.end()) {
                        try {
                            it->second.set_exception(
                                std::make_exception_ptr(std::runtime_error("Request timeout"))
                            );
                        } catch (...) {
                            // promise가 이미 set된 경우 무시
                        }
                        pending_requests.erase(it);
                    }
                }

                return nullptr;
            }

            // 응답 받기
            protocol::coordinator_node::NetworkMessage response = result.future.get();

            // NetworkMessage → BaseResponse 변환
            return ConvertFromNetworkMessage(response);

        } catch (const std::exception& e) {
            std::cerr << "[NodeTcpClient] SendRequest exception: " << e.what() << std::endl;
            return nullptr;
        }
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
        size_t bytes_sent = 0;
        
        utils::SocketIOResult result = utils::SendExact(
            connection_info.node_socket,
            data,
            length,
            &bytes_sent
        );

        if (result != utils::SocketIOResult::SUCCESS) {
            std::cerr << "[NodeTcpClient] Send failed: " 
                      << utils::ToString(result)
                      << " (sent: " << bytes_sent << "/" << length << " bytes)" << std::endl;
            return false;
        }

        return true;
    }

    bool NodeTcpClient::ReceiveRaw(void* buffer, size_t length) {
        size_t bytes_received = 0;
        
        utils::SocketIOResult result = utils::ReceiveExact(
            connection_info.node_socket,
            buffer,
            length,
            &bytes_received
        );

        if (result != utils::SocketIOResult::SUCCESS) {
            if (result == utils::SocketIOResult::CONNECTION_CLOSED) {
                std::cout << "[NodeTcpClient] Connection closed by Node server" << std::endl;
            } else {
                std::cerr << "[NodeTcpClient] Receive failed: " 
                          << utils::ToString(result)
                          << " (received: " << bytes_received << "/" << length << " bytes)" << std::endl;
            }
            return false;
        }

        return true;
    }

    void NodeTcpClient::NotifyError(NetworkError error, const std::string& message) {
        if (error_callback) {
            error_callback(connection_info.node_id, error, message);
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