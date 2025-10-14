// src/coordinator/network/wallet_server/src/HttpParser.cpp
#include "coordinator/network/wallet_server/include/HttpParser.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>

namespace mpc_engine::coordinator::network::wallet_server
{
    TlsError HttpParser::ReceiveRequest(TlsConnection& conn, HttpRequest& request) 
    {
        request.Clear();
        
        // Step 1: 헤더 읽기 (청크 단위로 "\r\n\r\n" 찾기)
        char header_buffer[MAX_HEADER_SIZE];
        size_t total_read = 0;
        bool headers_complete = false;
        size_t headers_end = 0;
        int consecutive_want_read = 0;
        const int MAX_WANT_READ_RETRIES = 100;

        while (total_read < MAX_HEADER_SIZE && !headers_complete) 
        {
            size_t bytes_read = 0;
            TlsError err = conn.Read(header_buffer + total_read, 
                                     MAX_HEADER_SIZE - total_read, 
                                     &bytes_read);

            // CONNECTION_CLOSED는 즉시 반환
            if (err == TlsError::CONNECTION_CLOSED) {
                return err;
            }

            // 다른 에러 (WANT_READ 제외)
            if (err != TlsError::NONE && err != TlsError::WANT_READ) {
                std::cerr << "[HttpParser] Header read error: " << TlsErrorToString(err) << std::endl;
                return err;
            }

            // 데이터 읽음
            if (bytes_read > 0) {
                total_read += bytes_read;
                consecutive_want_read = 0;  // 리셋

                // "\r\n\r\n" 찾기 (헤더 끝)
                for (size_t i = 0; i + 3 < total_read; ++i) {
                    if (header_buffer[i] == '\r' && header_buffer[i+1] == '\n' &&
                        header_buffer[i+2] == '\r' && header_buffer[i+3] == '\n') 
                    {
                        headers_end = i + 4;
                        headers_complete = true;
                        break;
                    }
                }

                // 헤더 완료
                if (headers_complete) {
                    break;
                }
            }

            // WANT_READ: 데이터가 아직 안 왔음
            if (err == TlsError::WANT_READ || bytes_read == 0) {
                consecutive_want_read++;
                
                if (consecutive_want_read > MAX_WANT_READ_RETRIES) {
                    std::cerr << "[HttpParser] Too many WANT_READ retries" << std::endl;
                    return TlsError::TIMEOUT;
                }
                
                // 짧은 대기 (busy waiting 방지)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }

        if (!headers_complete) {
            std::cerr << "[HttpParser] Headers incomplete after " << total_read << " bytes" << std::endl;
            return TlsError::SSL_ERROR;
        }

        // Step 2: 헤더 파싱
        std::string_view header_view(header_buffer, headers_end);
        size_t line_start = 0;
        bool first_line = true;

        while (line_start < header_view.size()) 
        {
            size_t line_end = header_view.find("\r\n", line_start);
            if (line_end == std::string_view::npos) {
                break;
            }

            std::string_view line = header_view.substr(line_start, line_end - line_start);
            
            if (line.empty()) {
                break;
            }

            if (first_line) {
                if (!ParseRequestLine(line, request)) {
                    std::cerr << "[HttpParser] Invalid request line" << std::endl;
                    return TlsError::SSL_ERROR;
                }
                first_line = false;
            } else {
                ParseHeader(line, request);
            }

            line_start = line_end + 2;
        }

        // Step 3: Body 읽기 (Content-Length만큼)
        if (request.content_length > 0) 
        {
            if (request.content_length > MAX_BODY_SIZE) {
                std::cerr << "[HttpParser] Body too large: " << request.content_length << std::endl;
                return TlsError::SSL_ERROR;
            }

            std::cout << "[HttpParser] Reading body: " << request.content_length << " bytes" << std::endl;

            std::vector<char> body_buffer(request.content_length);
            TlsError err = conn.ReadExact(body_buffer.data(), request.content_length);
            
            if (err != TlsError::NONE) {
                std::cerr << "[HttpParser] Body read failed: " << TlsErrorToString(err) << std::endl;
                return err;
            }

            std::cout << "[HttpParser] Body read complete" << std::endl;

            // Protobuf 역직렬화
            request.protobuf_message = std::make_unique<WalletCoordinatorMessage>();
            if (!request.protobuf_message->ParseFromArray(body_buffer.data(), body_buffer.size())) {
                std::cerr << "[HttpParser] Protobuf parse failed" << std::endl;
                return TlsError::SSL_ERROR;
            }

            std::cout << "[HttpParser] Protobuf parsed successfully" << std::endl;
        }

        return TlsError::NONE;
    }

    TlsError HttpParser::SendResponse(TlsConnection& conn, const HttpResponse& response) 
    {
        // Step 1: Protobuf 직렬화
        std::string body;
        if (response.protobuf_message) {
            if (!response.protobuf_message->SerializeToString(&body)) {
                std::cerr << "[HttpParser] Protobuf serialization failed" << std::endl;
                return TlsError::SSL_ERROR;
            }
        }

        // Step 2: HTTP 헤더 생성
        char header_buffer[2048];
        size_t pos = 0;

        pos += Append(header_buffer + pos, "HTTP/1.1 ");
        pos += AppendInt(header_buffer + pos, response.status_code);
        pos += Append(header_buffer + pos, " ");
        pos += Append(header_buffer + pos, response.status_text);
        pos += Append(header_buffer + pos, "\r\n");

        pos += Append(header_buffer + pos, "Content-Type: ");
        pos += Append(header_buffer + pos, response.content_type);
        pos += Append(header_buffer + pos, "\r\n");

        pos += Append(header_buffer + pos, "Content-Length: ");
        pos += AppendInt(header_buffer + pos, body.size());
        pos += Append(header_buffer + pos, "\r\n");

        pos += Append(header_buffer + pos, "Connection: keep-alive\r\n\r\n");

        // Step 3: 헤더 전송
        TlsError err = conn.WriteExact(header_buffer, pos);
        if (err != TlsError::NONE) {
            std::cerr << "[HttpParser] Header send failed" << std::endl;
            return err;
        }

        // Step 4: Body 전송
        if (!body.empty()) {
            err = conn.WriteExact(body.data(), body.size());
            if (err != TlsError::NONE) {
                std::cerr << "[HttpParser] Body send failed" << std::endl;
                return err;
            }
        }

        return TlsError::NONE;
    }

    bool HttpParser::ParseRequestLine(std::string_view line, HttpRequest& request) 
    {
        size_t first_space = line.find(' ');
        if (first_space == std::string_view::npos) {
            return false;
        }

        size_t second_space = line.find(' ', first_space + 1);
        if (second_space == std::string_view::npos) {
            return false;
        }

        request.method = std::string(line.substr(0, first_space));
        request.path = std::string(line.substr(first_space + 1, second_space - first_space - 1));
        request.version = std::string(line.substr(second_space + 1));

        return true;
    }

    bool HttpParser::ParseHeader(std::string_view line, HttpRequest& request) 
    {
        size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            return false;
        }

        std::string_view key = TrimWhitespace(line.substr(0, colon));
        std::string_view value = TrimWhitespace(line.substr(colon + 1));

        if (key == "Content-Length") {
            try {
                request.content_length = std::stoull(std::string(value));
            } catch (...) {
                return false;
            }
        }
        else if (key == "Content-Type") {
            request.content_type = std::string(value);
        }

        return true;
    }

    std::string_view HttpParser::TrimWhitespace(std::string_view str) 
    {
        size_t start = 0;
        while (start < str.size() && std::isspace(str[start])) {
            ++start;
        }

        size_t end = str.size();
        while (end > start && std::isspace(str[end - 1])) {
            --end;
        }

        return str.substr(start, end - start);
    }

    size_t HttpParser::Append(char* dest, std::string_view src) 
    {
        std::memcpy(dest, src.data(), src.size());
        return src.size();
    }

    size_t HttpParser::AppendInt(char* dest, size_t value) 
    {
        return static_cast<size_t>(std::snprintf(dest, 32, "%zu", value));
    }

} // namespace mpc_engine::coordinator::network::wallet_server