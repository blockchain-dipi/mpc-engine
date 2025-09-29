// src/protocols/coordinator_wallet/include/WalletProtocolTypes.hpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace mpc_engine::protocol::coordinator_wallet
{
    // Wallet 서버와의 통신을 위한 메시지 타입
    enum class WalletMessageType : uint32_t 
    {
        SIGNING_REQUEST = 1001,
        SIGNING_RESPONSE = 1002,
        KEY_GENERATION_REQUEST = 1003,
        KEY_GENERATION_RESPONSE = 1004,
        STATUS_CHECK = 1005,
        STATUS_RESPONSE = 1006,
        THRESHOLD_UPDATE_REQUEST = 1007,
        THRESHOLD_UPDATE_RESPONSE = 1008
    };

    // HTTP 헤더 정보
    struct HttpHeaders 
    {
        std::string authorization;      // Bearer token
        std::string contentType = "application/json";
        std::string userAgent = "MPC-Engine/1.0";
        std::string requestId;          // 요청 추적용
        
        std::string ToString() const;
        void FromString(const std::string& headerStr);
    };

    // 기본 요청 구조
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

    // 기본 응답 구조
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

    // 서명 요청
    struct WalletSigningRequest : public WalletBaseRequest 
    {
        std::string keyId;
        std::string transactionData;
        uint32_t threshold = 2;
        uint32_t totalShards = 3;
        std::vector<std::string> requiredShards;  // 특정 샤드 지정
        
        WalletSigningRequest() : WalletBaseRequest(WalletMessageType::SIGNING_REQUEST) {}
        std::string ToJson() const override;
    };

    // 서명 응답
    struct WalletSigningResponse : public WalletBaseResponse 
    {
        std::string keyId;
        std::string finalSignature;
        std::vector<std::string> shardSignatures;
        uint32_t successfulShards = 0;
        
        WalletSigningResponse() : WalletBaseResponse(WalletMessageType::SIGNING_RESPONSE) {}
        bool FromJson(const std::string& json) override;
    };

    // 상태 확인 요청
    struct WalletStatusRequest : public WalletBaseRequest 
    {
        WalletStatusRequest() : WalletBaseRequest(WalletMessageType::STATUS_CHECK) {}
        std::string ToJson() const override;
    };

    // 상태 확인 응답
    struct WalletStatusResponse : public WalletBaseResponse 
    {
        struct NodeStatus {
            std::string nodeId;
            std::string platform;
            bool connected;
            uint32_t shardIndex;
            double responseTime;  // ms
        };
        
        std::vector<NodeStatus> nodes;
        uint32_t totalNodes = 0;
        uint32_t connectedNodes = 0;
        uint64_t uptimeSeconds = 0;
        
        WalletStatusResponse() : WalletBaseResponse(WalletMessageType::STATUS_RESPONSE) {}
        bool FromJson(const std::string& json) override;
    };

    // HTTP 요청 구조
    struct HttpRequest 
    {
        std::string method = "POST";
        std::string url;
        HttpHeaders headers;
        std::string body;
        uint32_t timeoutMs = 30000;
        
        std::string ToString() const;
    };

    // HTTP 응답 구조
    struct HttpResponse 
    {
        uint16_t statusCode = 0;
        std::string statusMessage;
        HttpHeaders headers;
        std::string body;
        uint32_t responseTimeMs = 0;
        
        bool IsSuccess() const { return statusCode >= 200 && statusCode < 300; }
        std::string ToString() const;
    };
}