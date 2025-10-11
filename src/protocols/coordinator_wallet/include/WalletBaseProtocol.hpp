// src/protocols/coordinator_wallet/include/WalletBaseProtocol.hpp
#pragma once
#include "protocols/coordinator_wallet/include/WalletProtocolTypes.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace mpc_engine::protocol::coordinator_wallet
{
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

} // namespace mpc_engine::protocol::coordinator_wallet