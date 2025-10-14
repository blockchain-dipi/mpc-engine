// src/common/network/tls/include/TlsConnection.hpp
#pragma once

#include "TlsContext.hpp"
#include "types/BasicTypes.hpp"
#include <string>
#include <memory>
#include <chrono>

namespace mpc_engine::network::tls
{
    /**
     * @brief TLS 연결 상태
     */
    enum class TlsConnectionState 
    {
        DISCONNECTED = 0,
        CONNECTING = 1,
        HANDSHAKING = 2,
        CONNECTED = 3,
        DISCONNECTING = 4,
        ERROR = 5
    };

    /**
     * @brief TLS 에러 타입
     */
    enum class TlsError 
    {
        NONE = 0,
        HANDSHAKE_FAILED = 1,
        CERTIFICATE_VERIFY_FAILED = 2,
        READ_FAILED = 3,
        WRITE_FAILED = 4,
        TIMEOUT = 5,
        CONNECTION_CLOSED = 6,
        WANT_READ = 7,   // 재시도 필요
        WANT_WRITE = 8,  // 재시도 필요
        SYSCALL_ERROR = 9,
        SSL_ERROR = 10
    };

    inline const char* TlsConnectionStateToString(TlsConnectionState state) 
    {
        switch (state) {
            case TlsConnectionState::DISCONNECTED: return "DISCONNECTED";
            case TlsConnectionState::CONNECTING: return "CONNECTING";
            case TlsConnectionState::HANDSHAKING: return "HANDSHAKING";
            case TlsConnectionState::CONNECTED: return "CONNECTED";
            case TlsConnectionState::DISCONNECTING: return "DISCONNECTING";
            case TlsConnectionState::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    inline const char* TlsErrorToString(TlsError error) 
    {
        switch (error) {
            case TlsError::NONE: return "NONE";
            case TlsError::HANDSHAKE_FAILED: return "HANDSHAKE_FAILED";
            case TlsError::CERTIFICATE_VERIFY_FAILED: return "CERTIFICATE_VERIFY_FAILED";
            case TlsError::READ_FAILED: return "READ_FAILED";
            case TlsError::WRITE_FAILED: return "WRITE_FAILED";
            case TlsError::TIMEOUT: return "TIMEOUT";
            case TlsError::CONNECTION_CLOSED: return "CONNECTION_CLOSED";
            case TlsError::WANT_READ: return "WANT_READ";
            case TlsError::WANT_WRITE: return "WANT_WRITE";
            case TlsError::SYSCALL_ERROR: return "SYSCALL_ERROR";
            case TlsError::SSL_ERROR: return "SSL_ERROR";
            default: return "UNKNOWN";
        }
    }

    /**
     * @brief TLS 연결 설정
     */
    struct TlsConnectionConfig 
    {
        uint32_t handshake_timeout_ms = 10000;  // 10초
        uint32_t read_timeout_ms = 30000;       // 30초
        uint32_t write_timeout_ms = 30000;      // 30초
        bool enable_sni = true;                 // SNI (Server Name Indication)
        std::string sni_hostname;               // SNI에 사용할 호스트명
    };

    /**
     * @brief TLS 연결 클래스
     * 
     * SSL 객체를 래핑하여 안전한 TLS 통신 제공
     * RAII 패턴으로 자동 정리
     */
    class TlsConnection 
    {
    private:
        SSL* ssl = nullptr;
        socket_t socket_fd = INVALID_SOCKET_VALUE;
        TlsConnectionState state = TlsConnectionState::DISCONNECTED;
        TlsConnectionConfig config;
        
        TlsError last_error = TlsError::NONE;
        std::string last_error_msg;
        
        bool is_client_mode = true;
        uint64_t connection_start_time = 0;
        uint64_t handshake_complete_time = 0;

    public:
        TlsConnection() = default;
        ~TlsConnection();

        // 복사 금지
        TlsConnection(const TlsConnection&) = delete;
        TlsConnection& operator=(const TlsConnection&) = delete;

        // 이동 허용
        TlsConnection(TlsConnection&& other) noexcept;
        TlsConnection& operator=(TlsConnection&& other) noexcept;

        /**
         * @brief 클라이언트 TLS 연결 시작
         * 
         * @param tls_ctx TLS Context
         * @param socket_fd 이미 연결된 TCP 소켓
         * @param config 연결 설정
         * @return 성공 시 true
         */
        bool ConnectClient(const TlsContext& tls_ctx, socket_t socket_fd, 
                          const TlsConnectionConfig& config = TlsConnectionConfig());

        /**
         * @brief 서버 TLS 연결 수락
         * 
         * @param tls_ctx TLS Context
         * @param socket_fd Accept된 TCP 소켓
         * @param config 연결 설정
         * @return 성공 시 true
         */
        bool AcceptServer(const TlsContext& tls_ctx, socket_t socket_fd,
                         const TlsConnectionConfig& config = TlsConnectionConfig());

        /**
         * @brief TLS 핸드셰이크 수행
         * 
         * @return 성공 시 true
         */
        bool DoHandshake();

        /**
         * @brief 데이터 읽기
         * 
         * @param buffer 데이터 버퍼
         * @param length 읽을 크기
         * @param bytes_read 실제 읽은 크기 (출력)
         * @return TlsError
         */
        TlsError Read(void* buffer, size_t length, size_t* bytes_read);

        /**
         * @brief 데이터 쓰기
         * 
         * @param data 데이터
         * @param length 쓸 크기
         * @param bytes_written 실제 쓴 크기 (출력)
         * @return TlsError
         */
        TlsError Write(const void* data, size_t length, size_t* bytes_written);

        /**
         * @brief 정확히 length 바이트 읽기
         */
        TlsError ReadExact(void* buffer, size_t length);

        /**
         * @brief 정확히 length 바이트 쓰기
         */
        TlsError WriteExact(const void* data, size_t length);

        /**
         * @brief 연결 종료 (Graceful Shutdown)
         */
        void Shutdown();

        /**
         * @brief 연결 강제 종료
         */
        void Close();

        // 상태 조회
        TlsConnectionState GetState() const { return state; }
        bool IsConnected() const { return state == TlsConnectionState::CONNECTED; }
        bool IsHandshaking() const { return state == TlsConnectionState::HANDSHAKING; }
        
        TlsError GetLastError() const { return last_error; }
        std::string GetLastErrorMessage() const { return last_error_msg; }

        /**
         * @brief 피어 인증서 정보 가져오기
         */
        std::string GetPeerCertificateInfo() const;

        /**
         * @brief 사용된 암호화 스위트 정보
         */
        std::string GetCipherInfo() const;

        /**
         * @brief TLS 프로토콜 버전
         */
        std::string GetProtocolVersion() const;

        /**
         * @brief 핸드셰이크 소요 시간 (ms)
         */
        uint64_t GetHandshakeDuration() const;

    private:
        bool Initialize(const TlsContext& tls_ctx, socket_t socket_fd, 
                       const TlsConnectionConfig& cfg, bool is_client);
        
        bool SetSocketNonBlocking(bool non_blocking);
        bool WaitForIO(bool wait_read, uint32_t timeout_ms);
        
        TlsError HandleSSLError(int ssl_return_code);
        void SetError(TlsError error, const std::string& message);
        void ClearError();

        static std::string GetSSLErrorString(int ssl_error);
    };

    /**
     * @brief TLS 연결 헬퍼 함수들
     */
    namespace TlsConnectionHelper 
    {
        /**
         * @brief 호스트명 검증 (SNI)
         */
        bool VerifyHostname(SSL* ssl, const std::string& hostname);

        /**
         * @brief 인증서 체인 출력 (디버그용)
         */
        void PrintCertificateChain(SSL* ssl);

        /**
         * @brief 연결 정보 출력
         */
        void PrintConnectionInfo(const TlsConnection& conn);
    }

} // namespace mpc_engine::network::tls