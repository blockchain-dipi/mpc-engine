// src/common/https/include/HttpsClient.hpp
#pragma once
#include "common/https/include/HttpWriter.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include "common/network/tls/include/TlsConnection.hpp"
#include <string>
#include <string_view>
#include <memory>
#include <functional>

namespace mpc_engine::https
{
    /**
     * @brief HTTPS Client 설정
     */
    struct HttpsClientConfig 
    {
        uint32_t connect_timeout_ms = 5000;
        uint32_t read_timeout_ms = 30000;
        uint32_t write_timeout_ms = 30000;
    };

    /**
     * @brief HTTPS Client
     * 
     * TLS 연결 + HTTP 요청/응답 통합
     * 인증서는 외부에서 로드하여 주입 (KMS 사용)
     */
    class HttpsClient 
    {
    private:
        network::tls::TlsConnection tls_conn_;
        HttpsClientConfig config_;
        bool is_connected_ = false;
        
        std::string current_host_;
        uint16_t current_port_ = 0;

    public:
        explicit HttpsClient(const HttpsClientConfig& config);
        ~HttpsClient();

        /**
         * @brief 연결 (TLS Context는 외부에서 제공)
         * 
         * @param tls_ctx 초기화된 TLS Context (CA 인증서 로드 완료)
         * @param host 호스트명
         * @param port 포트 (기본 443)
         * @return 성공 여부
         */
        bool Connect(
            const network::tls::TlsContext& tls_ctx,
            const std::string& host, 
            uint16_t port = 443
        );

        /**
         * @brief POST 요청 (JSON)
         */
        bool PostJson(
            std::string_view path,
            std::string_view auth_token,
            std::string_view request_id,
            std::string_view json_body,
            char* response_buffer,
            size_t buffer_size,
            HttpResponseReader::Response& response
        );

        /**
         * @brief GET 요청
         */
        bool Get(
            std::string_view path,
            std::string_view auth_token,
            std::string_view request_id,
            char* response_buffer,
            size_t buffer_size,
            HttpResponseReader::Response& response
        );

        /**
         * @brief 연결 종료
         */
        void Disconnect();

        bool IsConnected() const { return is_connected_; }
        const std::string& GetCurrentHost() const { return current_host_; }

    private:
        bool PerformRequest(
            std::function<network::tls::TlsError()> write_fn,
            char* response_buffer,
            size_t buffer_size,
            HttpResponseReader::Response& response
        );
    };

} // namespace mpc_engine::https