// src/protocols/coordinator_wallet/include/WalletBaseProtocol.hpp
#pragma once
#include "types/WalletMessageType.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace mpc_engine::protocol::coordinator_wallet
{
    struct WalletBaseRequest 
    {
        mpc_engine::WalletMessageType messageType;
        std::string requestId;
        std::string timestamp;
        std::string coordinatorId;
        
        WalletBaseRequest(mpc_engine::WalletMessageType type) : messageType(type) {}
        virtual ~WalletBaseRequest() = default;
        
        virtual std::string ToJson() const = 0;
    };
    
    struct WalletBaseResponse 
    {
        mpc_engine::WalletMessageType messageType;
        bool success = false;
        std::string errorMessage;
        std::string requestId;
        std::string timestamp;
        
        WalletBaseResponse(mpc_engine::WalletMessageType type) : messageType(type) {}
        virtual ~WalletBaseResponse() = default;
        
        virtual bool FromJson(const std::string& json) = 0;
    };

} // namespace mpc_engine::protocol::coordinator_wallet