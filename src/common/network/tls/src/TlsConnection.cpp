// src/common/network/tls/src/TlsConnection.cpp
#include "common/network/tls/include/TlsConnection.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

namespace mpc_engine::network::tls
{
    // ========================================
    // TlsConnection 구현
    // ========================================

    TlsConnection::~TlsConnection() 
    {
        Close();
    }

    TlsConnection::TlsConnection(TlsConnection&& other) noexcept
        : ssl(other.ssl)
        , socket_fd(other.socket_fd)
        , state(other.state)
        , config(std::move(other.config))
        , last_error(other.last_error)
        , last_error_msg(std::move(other.last_error_msg))
        , is_client_mode(other.is_client_mode)
        , connection_start_time(other.connection_start_time)
        , handshake_complete_time(other.handshake_complete_time)
    {
        other.ssl = nullptr;
        other.socket_fd = INVALID_SOCKET_VALUE;
        other.state = TlsConnectionState::DISCONNECTED;
    }

    TlsConnection& TlsConnection::operator=(TlsConnection&& other) noexcept 
    {
        if (this != &other) {
            Close();
            
            ssl = other.ssl;
            socket_fd = other.socket_fd;
            state = other.state;
            config = std::move(other.config);
            last_error = other.last_error;
            last_error_msg = std::move(other.last_error_msg);
            is_client_mode = other.is_client_mode;
            connection_start_time = other.connection_start_time;
            handshake_complete_time = other.handshake_complete_time;
            
            other.ssl = nullptr;
            other.socket_fd = INVALID_SOCKET_VALUE;
            other.state = TlsConnectionState::DISCONNECTED;
        }
        return *this;
    }

    bool TlsConnection::ConnectClient(const TlsContext& tls_ctx, socket_t sock_fd, 
                                      const TlsConnectionConfig& cfg) 
    {
        return Initialize(tls_ctx, sock_fd, cfg, true);
    }

    bool TlsConnection::AcceptServer(const TlsContext& tls_ctx, socket_t sock_fd,
                                     const TlsConnectionConfig& cfg) 
    {
        return Initialize(tls_ctx, sock_fd, cfg, false);
    }

    bool TlsConnection::Initialize(const TlsContext& tls_ctx, socket_t sock_fd, 
                                   const TlsConnectionConfig& cfg, bool is_client) 
    {
        if (state != TlsConnectionState::DISCONNECTED) {
            SetError(TlsError::SSL_ERROR, "Already connected");
            return false;
        }

        if (sock_fd == INVALID_SOCKET_VALUE) {
            SetError(TlsError::SYSCALL_ERROR, "Invalid socket");
            return false;
        }

        config = cfg;
        socket_fd = sock_fd;
        is_client_mode = is_client;
        connection_start_time = utils::GetCurrentTimeMs();

        // SSL 객체 생성
        ssl = tls_ctx.CreateSSL();
        if (!ssl) {
            SetError(TlsError::SSL_ERROR, "Failed to create SSL object");
            return false;
        }

        // 소켓 연결
        if (SSL_set_fd(ssl, socket_fd) != 1) {
            SetError(TlsError::SSL_ERROR, "Failed to set socket FD");
            SSL_free(ssl);
            ssl = nullptr;
            return false;
        }

        // 클라이언트 모드 설정
        if (is_client_mode) {
            SSL_set_connect_state(ssl);
            
            // SNI 설정
            if (config.enable_sni && !config.sni_hostname.empty()) {
                if (SSL_set_tlsext_host_name(ssl, config.sni_hostname.c_str()) != 1) {
                    std::cerr << "[TLS] Warning: Failed to set SNI hostname" << std::endl;
                }
            }
        } else {
            SSL_set_accept_state(ssl);
        }

        state = TlsConnectionState::CONNECTING;
        ClearError();

        return true;
    }

    bool TlsConnection::DoHandshake() 
    {
        if (state != TlsConnectionState::CONNECTING && 
            state != TlsConnectionState::HANDSHAKING) {
            SetError(TlsError::SSL_ERROR, "Invalid state for handshake");
            return false;
        }

        state = TlsConnectionState::HANDSHAKING;

        // 논블로킹 모드로 설정
        SetSocketNonBlocking(true);

        uint64_t start_time = utils::GetCurrentTimeMs();
        uint64_t deadline = start_time + config.handshake_timeout_ms;

        while (true) {
            int result = SSL_do_handshake(ssl);
            
            if (result == 1) {
                // 핸드셰이크 성공
                state = TlsConnectionState::CONNECTED;
                handshake_complete_time = utils::GetCurrentTimeMs();
                ClearError();                
                return true;
            }

            // 에러 처리
            int ssl_error = SSL_get_error(ssl, result);
            
            if (ssl_error == SSL_ERROR_WANT_READ) {
                // 읽기 대기
                uint32_t remaining = deadline > utils::GetCurrentTimeMs() 
                    ? deadline - utils::GetCurrentTimeMs() : 0;
                
                if (remaining == 0 || !WaitForIO(true, remaining)) {
                    SetError(TlsError::TIMEOUT, "Handshake timeout");
                    state = TlsConnectionState::ERROR;
                    return false;
                }
                continue;
            }
            
            if (ssl_error == SSL_ERROR_WANT_WRITE) {
                // 쓰기 대기
                uint32_t remaining = deadline > utils::GetCurrentTimeMs() 
                    ? deadline - utils::GetCurrentTimeMs() : 0;
                
                if (remaining == 0 || !WaitForIO(false, remaining)) {
                    SetError(TlsError::TIMEOUT, "Handshake timeout");
                    state = TlsConnectionState::ERROR;
                    return false;
                }
                continue;
            }

            // 실제 에러
            SetError(TlsError::HANDSHAKE_FAILED, 
                    "Handshake failed: " + GetSSLErrorString(ssl_error));
            state = TlsConnectionState::ERROR;
            
            std::cerr << "[TLS] Handshake error: " << last_error_msg << std::endl;
            return false;
        }
    }

    TlsError TlsConnection::Read(void* buffer, size_t length, size_t* bytes_read) 
    {
        if (bytes_read) {
            *bytes_read = 0;
        }

        if (state != TlsConnectionState::CONNECTED) {
            SetError(TlsError::SSL_ERROR, "Not connected");
            return last_error;
        }

        if (!buffer || length == 0) {
            SetError(TlsError::SSL_ERROR, "Invalid parameters");
            return last_error;
        }

        ClearError();

        int result = SSL_read(ssl, buffer, length);
        
        if (result > 0) {
            if (bytes_read) {
                *bytes_read = result;
            }
            return TlsError::NONE;
        }

        return HandleSSLError(result);
    }

    TlsError TlsConnection::Write(const void* data, size_t length, size_t* bytes_written) 
    {
        if (bytes_written) {
            *bytes_written = 0;
        }

        if (state != TlsConnectionState::CONNECTED) {
            SetError(TlsError::SSL_ERROR, "Not connected");
            return last_error;
        }

        if (!data || length == 0) {
            SetError(TlsError::SSL_ERROR, "Invalid parameters");
            return last_error;
        }

        ClearError();

        int result = SSL_write(ssl, data, length);
        
        if (result > 0) {
            if (bytes_written) {
                *bytes_written = result;
            }
            return TlsError::NONE;
        }

        return HandleSSLError(result);
    }

    TlsError TlsConnection::ReadExact(void* buffer, size_t length) 
    {
        size_t total_read = 0;
        char* buf = static_cast<char*>(buffer);

        while (total_read < length) {
            size_t bytes_read = 0;
            TlsError error = Read(buf + total_read, length - total_read, &bytes_read);
            
            if (error != TlsError::NONE && error != TlsError::WANT_READ) {
                return error;
            }

            if (error == TlsError::WANT_READ) {
                if (!WaitForIO(true, config.read_timeout_ms)) {
                    SetError(TlsError::TIMEOUT, "Read timeout");
                    return TlsError::TIMEOUT;
                }
                continue;
            }

            total_read += bytes_read;
        }

        return TlsError::NONE;
    }

    TlsError TlsConnection::WriteExact(const void* data, size_t length) 
    {
        size_t total_written = 0;
        const char* buf = static_cast<const char*>(data);

        while (total_written < length) {
            size_t bytes_written = 0;
            TlsError error = Write(buf + total_written, length - total_written, &bytes_written);
            
            if (error != TlsError::NONE && error != TlsError::WANT_WRITE) {
                return error;
            }

            if (error == TlsError::WANT_WRITE) {
                if (!WaitForIO(false, config.write_timeout_ms)) {
                    SetError(TlsError::TIMEOUT, "Write timeout");
                    return TlsError::TIMEOUT;
                }
                continue;
            }

            total_written += bytes_written;
        }

        return TlsError::NONE;
    }

    void TlsConnection::Shutdown() 
    {
        if (state != TlsConnectionState::CONNECTED) {
            return;
        }

        state = TlsConnectionState::DISCONNECTING;

        if (ssl) {
            // Graceful shutdown (양방향 종료)
            int result = SSL_shutdown(ssl);
            if (result == 0) {
                // 첫 번째 단계만 완료, 두 번째 시도
                SSL_shutdown(ssl);
            }
        }

        Close();
    }

    void TlsConnection::Close() 
    {
        if (ssl) {
            SSL_free(ssl);
            ssl = nullptr;
        }

        // 소켓은 외부에서 관리하므로 닫지 않음
        socket_fd = INVALID_SOCKET_VALUE;
        state = TlsConnectionState::DISCONNECTED;
    }

    std::string TlsConnection::GetPeerCertificateInfo() const 
    {
        if (!ssl || state != TlsConnectionState::CONNECTED) {
            return "Not connected";
        }

        X509* cert = SSL_get_peer_certificate(ssl);
        if (!cert) {
            return "No peer certificate";
        }

        char subject[256];
        X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject));
        
        char issuer[256];
        X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer));

        X509_free(cert);

        return std::string("Subject: ") + subject + ", Issuer: " + issuer;
    }

    std::string TlsConnection::GetCipherInfo() const 
    {
        if (!ssl || state != TlsConnectionState::CONNECTED) {
            return "N/A";
        }

        const char* cipher = SSL_get_cipher(ssl);
        return cipher ? cipher : "Unknown";
    }

    std::string TlsConnection::GetProtocolVersion() const 
    {
        if (!ssl || state != TlsConnectionState::CONNECTED) {
            return "N/A";
        }

        const char* version = SSL_get_version(ssl);
        return version ? version : "Unknown";
    }

    uint64_t TlsConnection::GetHandshakeDuration() const 
    {
        if (handshake_complete_time == 0 || connection_start_time == 0) {
            return 0;
        }
        return handshake_complete_time - connection_start_time;
    }

    bool TlsConnection::SetSocketNonBlocking(bool non_blocking) 
    {
        int flags = fcntl(socket_fd, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }

        if (non_blocking) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }

        return fcntl(socket_fd, F_SETFL, flags) == 0;
    }

    bool TlsConnection::WaitForIO(bool wait_read, uint32_t timeout_ms) 
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(socket_fd, &fds);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int result;
        if (wait_read) {
            result = select(socket_fd + 1, &fds, nullptr, nullptr, &tv);
        } else {
            result = select(socket_fd + 1, nullptr, &fds, nullptr, &tv);
        }

        return result > 0;
    }

    TlsError TlsConnection::HandleSSLError(int ssl_return_code) 
    {
        int ssl_error = SSL_get_error(ssl, ssl_return_code);

        switch (ssl_error) {
            case SSL_ERROR_NONE:
                return TlsError::NONE;

            case SSL_ERROR_WANT_READ:
                return TlsError::WANT_READ;

            case SSL_ERROR_WANT_WRITE:
                return TlsError::WANT_WRITE;

            case SSL_ERROR_ZERO_RETURN:
                SetError(TlsError::CONNECTION_CLOSED, "Connection closed by peer");
                state = TlsConnectionState::DISCONNECTED;
                return TlsError::CONNECTION_CLOSED;

            case SSL_ERROR_SYSCALL:
                SetError(TlsError::SYSCALL_ERROR, 
                        "System call error: " + std::string(strerror(errno)));
                state = TlsConnectionState::ERROR;
                return TlsError::SYSCALL_ERROR;

            case SSL_ERROR_SSL:
                SetError(TlsError::SSL_ERROR, 
                        "SSL protocol error: " + GetSSLErrorString(ssl_error));
                state = TlsConnectionState::ERROR;
                return TlsError::SSL_ERROR;

            default:
                SetError(TlsError::SSL_ERROR, 
                        "Unknown SSL error: " + std::to_string(ssl_error));
                state = TlsConnectionState::ERROR;
                return TlsError::SSL_ERROR;
        }
    }

    void TlsConnection::SetError(TlsError error, const std::string& message) 
    {
        last_error = error;
        last_error_msg = message;
    }

    void TlsConnection::ClearError() 
    {
        last_error = TlsError::NONE;
        last_error_msg.clear();
    }

    std::string TlsConnection::GetSSLErrorString(int ssl_error) 
    {
        char buf[256];
        ERR_error_string_n(static_cast<unsigned long>(ssl_error), buf, sizeof(buf));
        return std::string(buf);
    }

    // ========================================
    // Helper 함수들
    // ========================================

    namespace TlsConnectionHelper 
    {
        bool VerifyHostname(SSL* ssl, const std::string& hostname) 
        {
            if (!ssl || hostname.empty()) {
                return false;
            }

            X509* cert = SSL_get_peer_certificate(ssl);
            if (!cert) {
                return false;
            }

            // TODO: 실제 hostname 검증 구현
            // X509_check_host() 사용

            X509_free(cert);
            return true;
        }

        void PrintCertificateChain(SSL* ssl) 
        {
            if (!ssl) {
                return;
            }

            STACK_OF(X509)* chain = SSL_get_peer_cert_chain(ssl);
            if (!chain) {
                std::cout << "No certificate chain" << std::endl;
                return;
            }

            int count = sk_X509_num(chain);
            std::cout << "Certificate chain (" << count << " certificates):" << std::endl;

            for (int i = 0; i < count; ++i) {
                X509* cert = sk_X509_value(chain, i);
                char subject[256];
                X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject));
                std::cout << "  [" << i << "] " << subject << std::endl;
            }
        }

        void PrintConnectionInfo(const TlsConnection& conn) 
        {
            std::cout << "=== TLS Connection Info ===" << std::endl;
            std::cout << "State: " << TlsConnectionStateToString(conn.GetState()) << std::endl;
            std::cout << "Protocol: " << conn.GetProtocolVersion() << std::endl;
            std::cout << "Cipher: " << conn.GetCipherInfo() << std::endl;
            std::cout << "Handshake duration: " << conn.GetHandshakeDuration() << "ms" << std::endl;
            std::cout << "Peer certificate: " << conn.GetPeerCertificateInfo() << std::endl;
        }
    }

} // namespace mpc_engine::network::tls