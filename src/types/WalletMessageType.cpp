// src/types/WalletMessageType.cpp
#include "WalletMessageType.hpp"
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