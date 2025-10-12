// src/protocols/coordinator_wallet/include/WalletSigningProtocol.hpp
#pragma once
#include "WalletBaseProtocol.hpp"
#include <memory>

namespace mpc_engine::protocol::coordinator_wallet
{
    struct WalletSigningRequest : public WalletBaseRequest 
    {
        std::string keyId;
        std::string transactionData;
        uint32_t threshold = 2;
        uint32_t totalShards = 3;
        std::vector<std::string> requiredShards;
        
        WalletSigningRequest() : WalletBaseRequest(mpc_engine::WalletMessageType::SIGNING_REQUEST) {}        
        std::string ToJson() const override;
    };

    struct WalletSigningResponse : public WalletBaseResponse 
    {
        std::string keyId;
        std::string finalSignature;
        std::vector<std::string> shardSignatures;
        uint32_t successfulShards = 0;
        
        WalletSigningResponse() : WalletBaseResponse(mpc_engine::WalletMessageType::SIGNING_REQUEST) {}        
        bool FromJson(const std::string& json) override;
    };
}