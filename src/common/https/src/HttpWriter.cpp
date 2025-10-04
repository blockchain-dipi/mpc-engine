// src/common/https/src/HttpWriter.cpp
#include "common/https/include/HttpWriter.hpp"
#include <cstdio>
#include <cstring>

namespace mpc_engine::https
{
    using namespace network::tls;

    // ========================================
    // HttpWriter 구현
    // ========================================

    TlsError HttpWriter::WritePostJson(
        TlsConnection& conn,
        std::string_view host,
        std::string_view path,
        std::string_view auth_token,
        std::string_view request_id,
        std::string_view json_body
    ) {
        char buffer[BUFFER_SIZE];
        size_t pos = 0;

        // Request Line: POST /path HTTP/1.1\r\n
        pos += Append(buffer + pos, "POST ");
        pos += Append(buffer + pos, path);
        pos += Append(buffer + pos, " HTTP/1.1\r\n");

        // Host: hostname\r\n
        pos += Append(buffer + pos, "Host: ");
        pos += Append(buffer + pos, host);
        pos += Append(buffer + pos, "\r\n");

        // Authorization: token\r\n
        if (!auth_token.empty()) {
            pos += Append(buffer + pos, "Authorization: ");
            pos += Append(buffer + pos, auth_token);
            pos += Append(buffer + pos, "\r\n");
        }

        // X-Request-ID: id\r\n
        if (!request_id.empty()) {
            pos += Append(buffer + pos, "X-Request-ID: ");
            pos += Append(buffer + pos, request_id);
            pos += Append(buffer + pos, "\r\n");
        }

        // Content-Type: application/json\r\n
        pos += Append(buffer + pos, "Content-Type: application/json\r\n");

        // User-Agent: MPC-Engine/1.0\r\n
        pos += Append(buffer + pos, "User-Agent: MPC-Engine/1.0\r\n");

        // Accept: */*\r\n
        pos += Append(buffer + pos, "Accept: */*\r\n");

        // Connection: keep-alive\r\n
        pos += Append(buffer + pos, "Connection: keep-alive\r\n");

        // Content-Length: N\r\n
        pos += Append(buffer + pos, "Content-Length: ");
        pos += AppendInt(buffer + pos, json_body.size());
        pos += Append(buffer + pos, "\r\n\r\n");

        // 헤더 전송
        TlsError err = conn.WriteExact(buffer, pos);
        if (err != TlsError::NONE) {
            return err;
        }

        // Body 전송 (복사 없이 직접)
        if (!json_body.empty()) {
            return conn.WriteExact(json_body.data(), json_body.size());
        }

        return TlsError::NONE;
    }

    TlsError HttpWriter::WriteGet(
        TlsConnection& conn,
        std::string_view host,
        std::string_view path,
        std::string_view auth_token,
        std::string_view request_id
    ) {
        char buffer[BUFFER_SIZE];
        size_t pos = 0;

        // Request Line: GET /path HTTP/1.1\r\n
        pos += Append(buffer + pos, "GET ");
        pos += Append(buffer + pos, path);
        pos += Append(buffer + pos, " HTTP/1.1\r\n");

        // Host: hostname\r\n
        pos += Append(buffer + pos, "Host: ");
        pos += Append(buffer + pos, host);
        pos += Append(buffer + pos, "\r\n");

        // Authorization: token\r\n
        if (!auth_token.empty()) {
            pos += Append(buffer + pos, "Authorization: ");
            pos += Append(buffer + pos, auth_token);
            pos += Append(buffer + pos, "\r\n");
        }

        // X-Request-ID: id\r\n
        if (!request_id.empty()) {
            pos += Append(buffer + pos, "X-Request-ID: ");
            pos += Append(buffer + pos, request_id);
            pos += Append(buffer + pos, "\r\n");
        }

        // User-Agent: MPC-Engine/1.0\r\n
        pos += Append(buffer + pos, "User-Agent: MPC-Engine/1.0\r\n");

        // Accept: */*\r\n
        pos += Append(buffer + pos, "Accept: */*\r\n");

        // Connection: keep-alive\r\n\r\n
        pos += Append(buffer + pos, "Connection: keep-alive\r\n\r\n");

        return conn.WriteExact(buffer, pos);
    }

    size_t HttpWriter::AppendInt(char* buf, size_t value) {
        return static_cast<size_t>(std::snprintf(buf, 32, "%zu", value));
    }

    // ========================================
    // HttpResponseReader 구현
    // ========================================

    TlsError HttpResponseReader::ReadResponse(
        TlsConnection& conn,
        char* buffer,
        size_t buffer_size,
        Response& response
    ) {
        size_t total_read = 0;
        bool headers_complete = false;
        size_t headers_end = 0;

        // 헤더 읽기 (최대 4KB 예상)
        while (total_read < buffer_size && !headers_complete) {
            size_t bytes_read = 0;
            TlsError err = conn.Read(buffer + total_read, 
                                     buffer_size - total_read, 
                                     &bytes_read);

            if (err != TlsError::NONE && err != TlsError::WANT_READ) {
                return err;
            }

            if (bytes_read > 0) {
                total_read += bytes_read;

                // "\r\n\r\n" 찾기 (헤더 끝)
                for (size_t i = 0; i + 3 < total_read; ++i) {
                    if (buffer[i] == '\r' && buffer[i+1] == '\n' &&
                        buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                        headers_end = i + 4;
                        headers_complete = true;
                        break;
                    }
                }
            }

            if (err == TlsError::WANT_READ) {
                continue;
            }
        }

        if (!headers_complete) {
            return TlsError::TIMEOUT;
        }

        // Status Line 파싱
        size_t line_end = 0;
        for (size_t i = 0; i < headers_end; ++i) {
            if (buffer[i] == '\r' && buffer[i+1] == '\n') {
                line_end = i;
                break;
            }
        }

        if (!ParseStatusLine(buffer, line_end, response.status_code)) {
            return TlsError::SSL_ERROR;
        }

        // Content-Length 찾기
        size_t content_length = FindContentLength(buffer, headers_end);

        // Body 읽기
        size_t body_start = headers_end;
        size_t body_read = total_read - headers_end;

        // 나머지 Body 읽기
        while (body_read < content_length && total_read < buffer_size) {
            size_t bytes_read = 0;
            TlsError err = conn.Read(buffer + total_read, 
                                     buffer_size - total_read, 
                                     &bytes_read);

            if (err != TlsError::NONE && err != TlsError::WANT_READ) {
                return err;
            }

            if (bytes_read > 0) {
                total_read += bytes_read;
                body_read += bytes_read;
            }
        }

        // 응답 설정 (버퍼 내 포인터만 저장, 복사 없음)
        response.body = buffer + body_start;
        response.body_len = body_read;
        response.success = (response.status_code >= 200 && response.status_code < 300);

        return TlsError::NONE;
    }

    bool HttpResponseReader::ParseStatusLine(const char* line, size_t len, int& status_code) {
        // "HTTP/1.1 200 OK" 파싱
        const char* p = line;
        const char* end = line + len;

        // "HTTP/1.1 " 스킵
        while (p < end && *p != ' ') ++p;
        if (p >= end) return false;
        ++p;

        // Status code 읽기 (숫자만)
        status_code = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            status_code = status_code * 10 + (*p - '0');
            ++p;
        }

        return status_code > 0;
    }

    size_t HttpResponseReader::FindContentLength(const char* headers, size_t headers_len) {
        const char* p = headers;
        const char* end = headers + headers_len;

        // "Content-Length:" 또는 "content-length:" 찾기
        while (p < end - 16) {
            if ((p[0] == 'C' || p[0] == 'c') &&
                (p[1] == 'o' || p[1] == 'O') &&
                (p[2] == 'n' || p[2] == 'N') &&
                (p[3] == 't' || p[3] == 'T') &&
                (p[4] == 'e' || p[4] == 'E') &&
                (p[5] == 'n' || p[5] == 'N') &&
                (p[6] == 't' || p[6] == 'T') &&
                p[7] == '-' &&
                (p[8] == 'L' || p[8] == 'l') &&
                (p[9] == 'e' || p[9] == 'E') &&
                (p[10] == 'n' || p[10] == 'N') &&
                (p[11] == 'g' || p[11] == 'G') &&
                (p[12] == 't' || p[12] == 'T') &&
                (p[13] == 'h' || p[13] == 'H') &&
                p[14] == ':') {
                
                p += 15;  // "Content-Length:" 스킵
                
                // 공백 스킵
                while (p < end && (*p == ' ' || *p == '\t')) ++p;

                // 숫자 파싱
                size_t len = 0;
                while (p < end && *p >= '0' && *p <= '9') {
                    len = len * 10 + (*p - '0');
                    ++p;
                }
                return len;
            }
            ++p;
        }

        return 0;  // Content-Length 없으면 0
    }

} // namespace mpc_engine::https