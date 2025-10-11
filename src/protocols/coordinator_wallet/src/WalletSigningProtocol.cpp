// src/protocols/coordinator_wallet/src/WalletSigningProtocol.cpp
#include "protocols/coordinator_wallet/include/WalletSigningProtocol.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace mpc_engine::protocol::coordinator_wallet
{
    // ========================================
    // WalletSigningRequest
    // ========================================
    
    std::string WalletSigningRequest::ToJson() const 
    {
        json j;
        
        j["messageType"] = static_cast<uint32_t>(messageType);
        j["requestId"] = requestId;
        j["timestamp"] = timestamp;
        j["coordinatorId"] = coordinatorId;
        
        j["keyId"] = keyId;
        j["transactionData"] = transactionData;
        j["threshold"] = threshold;
        j["totalShards"] = totalShards;
        
        if (!requiredShards.empty()) {
            j["requiredShards"] = requiredShards;
        }
        
        return j.dump();
    }

    // ========================================
    // WalletSigningResponse
    // ========================================
    
    bool WalletSigningResponse::FromJson(const std::string& json_str) 
    {
        try {
            json j = json::parse(json_str);
            
            messageType = static_cast<WalletMessageType>(
                j["messageType"].get<uint32_t>()
            );
            success = j["success"].get<bool>();
            
            if (j.contains("errorMessage")) {
                errorMessage = j["errorMessage"].get<std::string>();
            }
            
            if (j.contains("requestId")) {
                requestId = j["requestId"].get<std::string>();
            }
            
            if (j.contains("timestamp")) {
                timestamp = j["timestamp"].get<std::string>();
            }
            
            if (success) {
                keyId = j["keyId"].get<std::string>();
                finalSignature = j["finalSignature"].get<std::string>();
                
                if (j.contains("shardSignatures")) {
                    shardSignatures = j["shardSignatures"]
                        .get<std::vector<std::string>>();
                }
                
                if (j.contains("successfulShards")) {
                    successfulShards = j["successfulShards"].get<uint32_t>();
                }
            }
            
            return true;
            
        } catch (const json::exception& e) {
            errorMessage = "JSON parsing failed: " + std::string(e.what());
            return false;
        }
    }
} // namespace mpc_engine::protocol::coordinator_wallet