// src/common/network/framing/http.hpp
#pragma once
#include <string>
#include <cstdint>
#include <sstream>

namespace mpc_engine::network::framing
{
    /**
     * @brief HTTP 헤더 정보
     */
    struct HttpHeaders 
    {
        std::string authorization;
        std::string contentType = "application/json";
        std::string userAgent = "MPC-Engine/1.0";
        std::string requestId;
        
        inline std::string ToString() const 
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

        inline void FromString(const std::string& headerStr) 
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
        
        inline std::string ToString() const 
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
        
        inline bool IsSuccess() const 
        {
            return statusCode >= 200 && statusCode < 300;
        }

        inline std::string ToString() const 
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
    };

} // namespace mpc_engine::network::framing