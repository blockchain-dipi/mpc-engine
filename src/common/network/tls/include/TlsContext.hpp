// src/common/network/tls/include/TlsContext.hpp
#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include <memory>
#include <vector>

namespace mpc_engine::network::tls
{
    /**
     * @brief TLS 모드 (서버/클라이언트)
     */
    enum class TlsMode 
    {
        CLIENT = 0,
        SERVER = 1
    };

    /**
     * @brief TLS 버전
     */
    enum class TlsVersion 
    {
        TLS_1_2 = 0,
        TLS_1_3 = 1
    };

    /**
     * @brief 인증서 데이터 구조체
     */
    struct CertificateData 
    {
        std::string certificate_pem;  // PEM 포맷 인증서 데이터
        std::string private_key_pem;  // PEM 포맷 개인키 데이터
        std::string ca_chain_pem;     // PEM 포맷 CA 체인 (선택)
        
        bool IsValid() const {
            return !certificate_pem.empty() && !private_key_pem.empty();
        }
    };

    /**
     * @brief TLS 설정 구조체
     */
    struct TlsConfig 
    {
        TlsMode mode = TlsMode::CLIENT;
        TlsVersion min_version = TlsVersion::TLS_1_2;
        
        // 암호화 스위트 (비어있으면 기본값 사용)
        std::string cipher_list;
        std::string cipher_suites;
        
        // 기타
        bool enable_session_cache = true;
        int verify_depth = 10;
        
        static TlsConfig CreateSecureClientConfig();
        static TlsConfig CreateSecureServerConfig();
    };

    /**
     * @brief TLS Context 래퍼 클래스
     * 
     * OpenSSL SSL_CTX를 RAII 방식으로 관리
     * 인증서는 외부에서 주입받음 (KMS, 파일, 환경변수 등)
     */
    class TlsContext 
    {
    private:
        SSL_CTX* ctx = nullptr;
        TlsConfig config;
        bool is_initialized = false;
        bool has_certificate = false;
        bool has_ca = false;

    public:
        TlsContext() = default;
        ~TlsContext();

        TlsContext(const TlsContext&) = delete;
        TlsContext& operator=(const TlsContext&) = delete;
        TlsContext(TlsContext&& other) noexcept;
        TlsContext& operator=(TlsContext&& other) noexcept;

        /**
         * @brief TLS Context 초기화
         */
        bool Initialize(const TlsConfig& config);

        /**
         * @brief 인증서 로드 (메모리에서)
         * 
         * @param cert_data 인증서 데이터 (PEM 포맷)
         * @return 성공 시 true
         * 
         * 사용 예:
         *   - KMS에서: cert_data.certificate_pem = kms->GetSecret("tls-cert");
         *   - 파일에서: cert_data.certificate_pem = ReadFile("cert.pem");
         */
        bool LoadCertificate(const CertificateData& cert_data);

        /**
         * @brief CA 인증서 로드 (메모리에서)
         * 
         * @param ca_pem CA 인증서 PEM 데이터
         * @return 성공 시 true
         */
        bool LoadCA(const std::string& ca_pem);

        /**
         * @brief CA 인증서 로드 (여러 개)
         */
        bool LoadCAChain(const std::vector<std::string>& ca_chain);

        /**
         * @brief SSL 객체 생성 (연결용)
         */
        SSL* CreateSSL() const;

        bool IsInitialized() const { return is_initialized; }
        bool HasCertificate() const { return has_certificate; }
        bool HasCA() const { return has_ca; }
        SSL_CTX* GetContext() const { return ctx; }
        const TlsConfig& GetConfig() const { return config; }

        static std::string GetLastError();
        static void GlobalInit();
        static void GlobalCleanup();

    private:
        bool InitializeClientContext();
        bool InitializeServerContext();
        bool LoadCertificateFromMemory(const std::string& cert_pem);
        bool LoadPrivateKeyFromMemory(const std::string& key_pem);
        bool LoadCAFromMemory(const std::string& ca_pem);
        bool SetCipherList();
        bool SetTlsVersion();
        bool ConfigureVerification();
        bool ConfigureSessionCache();

        static int VerifyCallback(int preverify_ok, X509_STORE_CTX* ctx);
    };

    namespace CipherSuites 
    {
        constexpr const char* STRONG_TLS_1_2 = 
            "ECDHE-ECDSA-AES256-GCM-SHA384:"
            "ECDHE-RSA-AES256-GCM-SHA384:"
            "ECDHE-ECDSA-AES128-GCM-SHA256:"
            "ECDHE-RSA-AES128-GCM-SHA256";

        constexpr const char* STRONG_TLS_1_3 = 
            "TLS_AES_256_GCM_SHA384:"
            "TLS_AES_128_GCM_SHA256:"
            "TLS_CHACHA20_POLY1305_SHA256";
    }

} // namespace mpc_engine::network::tls