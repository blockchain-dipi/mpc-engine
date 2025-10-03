// src/protocols/coordinator_wallet/src/WalletProtocolTypes.cpp
#include "protocols/coordinator_wallet/include/WalletProtocolTypes.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace mpc_engine::protocol::coordinator_wallet
{
    // ========================================
    // WalletMessageType 문자열 변환
    // ========================================
    
    const char* ToString(WalletMessageType type) 
    {
        switch (type) {
            case WalletMessageType::SIGNING_REQUEST:
                return "SIGNING_REQUEST";
            case WalletMessageType::STATUS_CHECK:
                return "STATUS_CHECK";
            default:
                return "UNKNOWN";
        }
    }

    // ========================================
    // HttpHeaders
    // ========================================
    
    std::string HttpHeaders::ToString() const 
    {
        std::ostringstream oss;
        
        if (!authorization.empty()) {
            oss << "Authorization: " << authorization << "\r\n";
        }
        
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "User-Agent: " << userAgent << "\r\n";
        
        if (!requestId.empty()) {
            oss << "X-Request-ID: " << requestId << "\r\n";
        }
        
        return oss.str();
    }

    void HttpHeaders::FromString(const std::string& headerStr) 
    {
        size_t pos = 0;
        
        while (pos < headerStr.size()) {
            size_t lineEnd = headerStr.find("\r\n", pos);
            if (lineEnd == std::string::npos) {
                break;
            }
            
            std::string line = headerStr.substr(pos, lineEnd - pos);
            
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                
                // 앞뒤 공백 제거
                size_t valueStart = value.find_first_not_of(" \t");
                if (valueStart != std::string::npos) {
                    value = value.substr(valueStart);
                }
                
                if (key == "Authorization") {
                    authorization = value;
                } else if (key == "Content-Type") {
                    contentType = value;
                } else if (key == "User-Agent") {
                    userAgent = value;
                } else if (key == "X-Request-ID") {
                    requestId = value;
                }
            }
            
            pos = lineEnd + 2;
        }
    }

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

    // ========================================
    // WalletStatusRequest
    // ========================================
    
    std::string WalletStatusRequest::ToJson() const 
    {
        json j;
        
        j["messageType"] = static_cast<uint32_t>(messageType);
        j["requestId"] = requestId;
        j["timestamp"] = timestamp;
        j["coordinatorId"] = coordinatorId;
        
        return j.dump();
    }

    // ========================================
    // WalletStatusResponse
    // ========================================
    
    bool WalletStatusResponse::FromJson(const std::string& json_str) 
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
                totalNodes = j["totalNodes"].get<uint32_t>();
                connectedNodes = j["connectedNodes"].get<uint32_t>();
                uptimeSeconds = j["uptimeSeconds"].get<uint64_t>();
                
                if (j.contains("nodes")) {
                    for (const auto& node_json : j["nodes"]) {
                        NodeStatus node;
                        node.nodeId = node_json["nodeId"].get<std::string>();
                        node.platform = node_json["platform"].get<std::string>();
                        node.connected = node_json["connected"].get<bool>();
                        node.shardIndex = node_json["shardIndex"].get<uint32_t>();
                        node.responseTime = node_json["responseTime"].get<double>();
                        
                        nodes.push_back(node);
                    }
                }
            }
            
            return true;
            
        } catch (const json::exception& e) {
            errorMessage = "JSON parsing failed: " + std::string(e.what());
            return false;
        }
    }

    // ========================================
    // HttpRequest
    // ========================================
    
    std::string HttpRequest::ToString() const 
    {
        std::ostringstream oss;
        
        // Request Line
        oss << method << " " << url << " HTTP/1.1\r\n";
        
        // Headers
        oss << headers.ToString();
        
        // Content-Length
        if (!body.empty()) {
            oss << "Content-Length: " << body.size() << "\r\n";
        }
        
        // Empty line
        oss << "\r\n";
        
        // Body
        if (!body.empty()) {
            oss << body;
        }
        
        return oss.str();
    }

    // ========================================
    // HttpResponse
    // ========================================
    
    bool HttpResponse::IsSuccess() const 
    {
        return statusCode >= 200 && statusCode < 300;
    }

    std::string HttpResponse::ToString() const 
    {
        std::ostringstream oss;
        
        oss << "HTTP Response [" << statusCode << " " << statusMessage << "]\n";
        oss << "Headers:\n" << headers.ToString();
        
        if (!body.empty()) {
            oss << "Body (" << body.size() << " bytes):\n";
            
            if (body.size() <= 200) {
                oss << body;
            } else {
                oss << body.substr(0, 200) << "...";
            }
        }
        
        return oss.str();
    }

} // namespace mpc_engine::protocol::coordinator_wallet