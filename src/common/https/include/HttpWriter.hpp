// src/common/https/include/HttpWriter.hpp
#pragma once
#include "common/network/tls/include/TlsConnection.hpp"
#include <string_view>
#include <cstring>

namespace mpc_engine::https
{
    /**
     * @brief Zero-Copy HTTP Writer
     * 
     * 힙 할당 없이 스택 버퍼에 직접 작성 후 TLS로 전송
     * 목표: 초당 100만+ 요청 처리
     */
    class HttpWriter 
    {
    public:
        /**
         * @brief POST 요청 전송 (JSON Body)
         * 
         * @param conn TLS 연결
         * @param host 호스트명 (예: "wallet-server.example.com")
         * @param path 경로 (예: "/api/v1/sign")
         * @param auth_token Authorization 헤더 값 (예: "Bearer token123")
         * @param request_id X-Request-ID 값
         * @param json_body JSON 본문 (이미 직렬화된 문자열)
         * @return TlsError
         */
        static network::tls::TlsError WritePostJson(
            network::tls::TlsConnection& conn,
            std::string_view host,
            std::string_view path,
            std::string_view auth_token,
            std::string_view request_id,
            std::string_view json_body
        );

        /**
         * @brief GET 요청 전송
         */
        static network::tls::TlsError WriteGet(
            network::tls::TlsConnection& conn,
            std::string_view host,
            std::string_view path,
            std::string_view auth_token,
            std::string_view request_id
        );

    private:
        static constexpr size_t BUFFER_SIZE = 8192;
        
        static inline size_t Append(char* buf, std::string_view str) {
            std::memcpy(buf, str.data(), str.size());
            return str.size();
        }
        
        static size_t AppendInt(char* buf, size_t value);
    };

    /**
     * @brief Zero-Copy HTTP Response Reader
     */
    class HttpResponseReader 
    {
    public:
        struct Response {
            int status_code = 0;
            const char* body = nullptr;  // 버퍼 내 포인터 (복사 없음)
            size_t body_len = 0;
            bool success = false;
        };

        /**
         * @brief HTTP 응답 읽기
         * 
         * @param conn TLS 연결
         * @param buffer 응답 저장 버퍼 (호출자 제공)
         * @param buffer_size 버퍼 크기
         * @param response 파싱 결과 (body는 buffer 내부 포인터)
         * @return TlsError
         */
        static network::tls::TlsError ReadResponse(
            network::tls::TlsConnection& conn,
            char* buffer,
            size_t buffer_size,
            Response& response
        );

    private:
        static bool ParseStatusLine(const char* line, size_t len, int& status_code);
        static size_t FindContentLength(const char* headers, size_t headers_len);
    };

} // namespace mpc_engine::https