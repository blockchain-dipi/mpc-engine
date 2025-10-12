// src/common/network/https/src/HttpsClient.cpp
#include "common/network/https/include/HttpsClient.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>

namespace mpc_engine::network::https
{
    using namespace network::tls;

    HttpsClient::HttpsClient(const HttpsClientConfig& config)
        : config_(config)
    {
    }

    HttpsClient::~HttpsClient() 
    {
        Disconnect();
    }

    bool HttpsClient::Connect(
        const TlsContext& tls_ctx,
        const std::string& host, 
        uint16_t port
    ) {
        // 이미 같은 호스트에 연결되어 있으면 재사용
        if (is_connected_ && current_host_ == host && current_port_ == port) {
            return true;
        }

        // 기존 연결 종료
        if (is_connected_) {
            Disconnect();
        }

        // TCP 소켓 생성
        socket_t sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            return false;
        }

        // 호스트 주소 해석
        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            close(sock_fd);
            return false;
        }

        struct sockaddr_in serv_addr;
        std::memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        serv_addr.sin_port = htons(port);

        // 타임아웃 설정
        struct timeval timeout;
        timeout.tv_sec = config_.connect_timeout_ms / 1000;
        timeout.tv_usec = (config_.connect_timeout_ms % 1000) * 1000;
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        // TCP 연결
        if (connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            close(sock_fd);
            return false;
        }

        // TLS 연결 설정
        TlsConnectionConfig tls_conn_config;
        tls_conn_config.handshake_timeout_ms = config_.connect_timeout_ms;
        tls_conn_config.read_timeout_ms = config_.read_timeout_ms;
        tls_conn_config.write_timeout_ms = config_.write_timeout_ms;
        tls_conn_config.enable_sni = true;
        tls_conn_config.sni_hostname = host;

        // TLS 핸드셰이크
        if (!tls_conn_.ConnectClient(tls_ctx, sock_fd, tls_conn_config)) {
            close(sock_fd);
            return false;
        }

        if (!tls_conn_.DoHandshake()) {
            close(sock_fd);
            return false;
        }

        current_host_ = host;
        current_port_ = port;
        is_connected_ = true;

        return true;
    }

    bool HttpsClient::PostJson(
        std::string_view path,
        std::string_view auth_token,
        std::string_view request_id,
        std::string_view json_body,
        char* response_buffer,
        size_t buffer_size,
        HttpResponseReader::Response& response
    ) {
        if (!is_connected_) {
            return false;
        }

        auto write_fn = [&]() -> TlsError {
            return HttpWriter::WritePostJson(
                tls_conn_,
                current_host_,
                path,
                auth_token,
                request_id,
                json_body
            );
        };

        return PerformRequest(write_fn, response_buffer, buffer_size, response);
    }

    bool HttpsClient::Get(
        std::string_view path,
        std::string_view auth_token,
        std::string_view request_id,
        char* response_buffer,
        size_t buffer_size,
        HttpResponseReader::Response& response
    ) {
        if (!is_connected_) {
            return false;
        }

        auto write_fn = [&]() -> TlsError {
            return HttpWriter::WriteGet(
                tls_conn_,
                current_host_,
                path,
                auth_token,
                request_id
            );
        };

        return PerformRequest(write_fn, response_buffer, buffer_size, response);
    }

    void HttpsClient::Disconnect() 
    {
        if (is_connected_) {
            tls_conn_.Shutdown();
            tls_conn_.Close();
            is_connected_ = false;
            current_host_.clear();
            current_port_ = 0;
        }
    }

    bool HttpsClient::PerformRequest(
        std::function<TlsError()> write_fn,
        char* response_buffer,
        size_t buffer_size,
        HttpResponseReader::Response& response
    ) {
        TlsError write_err = write_fn();
        if (write_err != TlsError::NONE) {
            return false;
        }

        TlsError read_err = HttpResponseReader::ReadResponse(
            tls_conn_,
            response_buffer,
            buffer_size,
            response
        );

        return read_err == TlsError::NONE;
    }

} // namespace mpc_engine::network::https