// src/coordinator/network/wallet_server/include/HttpParser.hpp
#pragma once

#include "common/network/tls/include/TlsConnection.hpp"
#include "proto/wallet_coordinator/generated/wallet_message.pb.h"
#include <string>
#include <string_view>

namespace mpc_engine::coordinator::network::wallet_server
{
    using namespace mpc_engine::network::tls;
    using namespace mpc_engine::proto::wallet_coordinator;

    struct HttpRequest 
    {
        std::string method;      // POST
        std::string path;        // /api/v1/signing
        std::string version;     // HTTP/1.1
        size_t content_length = 0;
        std::string content_type;
        std::unique_ptr<WalletCoordinatorMessage> protobuf_message;
        
        void Clear() {
            method.clear();
            path.clear();
            version.clear();
            content_length = 0;
            content_type.clear();
            protobuf_message.reset();
        }
    };

    struct HttpResponse 
    {
        int status_code = 200;
        std::string status_text = "OK";
        std::string content_type = "application/protobuf";
        std::unique_ptr<WalletCoordinatorMessage> protobuf_message;
        
        void Clear() {
            status_code = 200;
            status_text = "OK";
            content_type = "application/protobuf";
            protobuf_message.reset();
        }
    };

    class HttpParser 
    {
    public:
        /**
         * @brief HTTP 요청 수신 및 파싱 (TLS)
         * 
         * @param conn TLS 연결
         * @param request 파싱된 요청 (출력)
         * @return TlsError
         */
        static TlsError ReceiveRequest(TlsConnection& conn, HttpRequest& request);

        /**
         * @brief HTTP 응답 전송 (TLS)
         * 
         * @param conn TLS 연결
         * @param response 전송할 응답
         * @return TlsError
         */
        static TlsError SendResponse(TlsConnection& conn, const HttpResponse& response);

    private:
        static bool ParseRequestLine(std::string_view line, HttpRequest& request);
        static bool ParseHeader(std::string_view line, HttpRequest& request);
        
        static constexpr size_t MAX_HEADER_SIZE = 8192;    // 8KB
        static constexpr size_t MAX_BODY_SIZE = 10485760;  // 10MB
        
        static std::string_view TrimWhitespace(std::string_view str);
        static size_t Append(char* dest, std::string_view src);
        static size_t AppendInt(char* dest, size_t value);
    };

} // namespace mpc_engine::coordinator::network::wallet_server