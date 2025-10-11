// src/protocols/coordinator_wallet/include/WalletProtocolTypes.hpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace mpc_engine::protocol::coordinator_wallet
{
    /**
     * @brief Wallet 서버와의 통신 프로토콜 타입
     */
    enum class WalletMessageType : uint32_t 
    {
        SIGNING_REQUEST = 1001,    // 서명 요청 프로토콜
        STATUS_CHECK = 1002,       // 상태 확인 프로토콜
        MAX_MESSAGE_TYPE
    };

    const char* ToString(WalletMessageType type);

    /**
     * @brief HTTP 헤더 정보
     */
    struct HttpHeaders 
    {
        std::string authorization;
        std::string contentType = "application/json";
        std::string userAgent = "MPC-Engine/1.0";
        std::string requestId;
        
        std::string ToString() const;
        void FromString(const std::string& headerStr);
    };

    /**
     * @brief 기본 요청 구조
     */
    struct WalletBaseRequest 
    {
        WalletMessageType messageType;
        std::string requestId;
        std::string timestamp;
        std::string coordinatorId;
        
        WalletBaseRequest(WalletMessageType type) : messageType(type) {}
        virtual ~WalletBaseRequest() = default;
        
        virtual std::string ToJson() const = 0;
    };

    /**
     * @brief 기본 응답 구조
     */
    struct WalletBaseResponse 
    {
        WalletMessageType messageType;
        bool success = false;
        std::string errorMessage;
        std::string requestId;
        std::string timestamp;
        
        WalletBaseResponse(WalletMessageType type) : messageType(type) {}
        virtual ~WalletBaseResponse() = default;
        
        virtual bool FromJson(const std::string& json) = 0;
    };

    /**
     * @brief HTTP 요청 구조
     */
    struct HttpRequest 
    {
        std::string method = "POST";
        std::string url;
        HttpHeaders headers;
        std::string body;
        uint32_t timeoutMs = 30000;
        
        std::string ToString() const;
    };

    /**
     * @brief HTTP 응답 구조
     */
    struct HttpResponse 
    {
        uint16_t statusCode = 0;
        std::string statusMessage;
        HttpHeaders headers;
        std::string body;
        uint32_t responseTimeMs = 0;
        
        bool IsSuccess() const;
        std::string ToString() const;
    };

} // namespace mpc_engine::protocol::coordinator_wallet