// src/common/network/tls/src/TlsContext.cpp
#include "common/network/tls/include/TlsContext.hpp"
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <iostream>
#include <cstring>

namespace mpc_engine::network::tls
{
    // ========================================
    // TlsConfig 구현
    // ========================================

    TlsConfig TlsConfig::CreateSecureClientConfig() 
    {
        TlsConfig config;
        config.mode = TlsMode::CLIENT;
        config.min_version = TlsVersion::TLS_1_2;
        config.cipher_list = CipherSuites::STRONG_TLS_1_2;
        config.cipher_suites = CipherSuites::STRONG_TLS_1_3;
        return config;
    }

    TlsConfig TlsConfig::CreateSecureServerConfig() 
    {
        TlsConfig config;
        config.mode = TlsMode::SERVER;
        config.min_version = TlsVersion::TLS_1_2;
        config.cipher_list = CipherSuites::STRONG_TLS_1_2;
        config.cipher_suites = CipherSuites::STRONG_TLS_1_3;
        return config;
    }

    // ========================================
    // TlsContext 구현
    // ========================================

    TlsContext::~TlsContext() 
    {
        if (ctx) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }

    TlsContext::TlsContext(TlsContext&& other) noexcept
        : ctx(other.ctx)
        , config(std::move(other.config))
        , is_initialized(other.is_initialized)
        , has_certificate(other.has_certificate)
        , has_ca(other.has_ca)
    {
        other.ctx = nullptr;
        other.is_initialized = false;
        other.has_certificate = false;
        other.has_ca = false;
    }

    TlsContext& TlsContext::operator=(TlsContext&& other) noexcept 
    {
        if (this != &other) {
            if (ctx) {
                SSL_CTX_free(ctx);
            }
            
            ctx = other.ctx;
            config = std::move(other.config);
            is_initialized = other.is_initialized;
            has_certificate = other.has_certificate;
            has_ca = other.has_ca;
            
            other.ctx = nullptr;
            other.is_initialized = false;
            other.has_certificate = false;
            other.has_ca = false;
        }
        return *this;
    }

    bool TlsContext::Initialize(const TlsConfig& cfg) 
    {
        if (is_initialized) {
            std::cerr << "[TLS] Already initialized" << std::endl;
            return false;
        }

        config = cfg;

        bool success = false;
        if (config.mode == TlsMode::CLIENT) {
            success = InitializeClientContext();
        } else {
            success = InitializeServerContext();
        }

        if (!success) {
            return false;
        }

        if (!SetTlsVersion()) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
            return false;
        }

        if (!SetCipherList()) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
            return false;
        }

        if (!ConfigureVerification()) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
            return false;
        }

        if (config.enable_session_cache && !ConfigureSessionCache()) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
            return false;
        }

        is_initialized = true;
        return true;
    }

    bool TlsContext::LoadCertificate(const CertificateData& cert_data) 
    {
        if (!is_initialized) {
            std::cerr << "[TLS] Context not initialized" << std::endl;
            return false;
        }

        if (!cert_data.IsValid()) {
            std::cerr << "[TLS] Invalid certificate data" << std::endl;
            return false;
        }

        // 인증서 로드
        if (!LoadCertificateFromMemory(cert_data.certificate_pem)) {
            return false;
        }

        // 개인키 로드
        if (!LoadPrivateKeyFromMemory(cert_data.private_key_pem)) {
            return false;
        }

        // 키-인증서 매칭 확인
        if (!SSL_CTX_check_private_key(ctx)) {
            std::cerr << "[TLS] Private key does not match certificate" << std::endl;
            return false;
        }

        // CA 체인 로드 (선택)
        if (!cert_data.ca_chain_pem.empty()) {
            if (!LoadCAFromMemory(cert_data.ca_chain_pem)) {
                std::cerr << "[TLS] Warning: Failed to load CA chain" << std::endl;
                // CA 체인은 선택사항이므로 실패해도 계속 진행
            }
        }

        has_certificate = true;
        return true;
    }

    bool TlsContext::LoadCA(const std::string& ca_pem) 
    {
        if (!is_initialized) {
            std::cerr << "[TLS] Context not initialized" << std::endl;
            return false;
        }

        if (ca_pem.empty()) {
            std::cerr << "[TLS] Empty CA data" << std::endl;
            return false;
        }

        if (!LoadCAFromMemory(ca_pem)) {
            return false;
        }

        has_ca = true;
        return true;
    }

    bool TlsContext::LoadCAChain(const std::vector<std::string>& ca_chain) 
    {
        if (!is_initialized) {
            std::cerr << "[TLS] Context not initialized" << std::endl;
            return false;
        }

        for (size_t i = 0; i < ca_chain.size(); ++i) {
            if (!LoadCAFromMemory(ca_chain[i])) {
                std::cerr << "[TLS] Failed to load CA #" << i << std::endl;
                return false;
            }
        }

        has_ca = true;
        return true;
    }

    SSL* TlsContext::CreateSSL() const 
    {
        if (!is_initialized || !ctx) {
            std::cerr << "[TLS] Context not initialized" << std::endl;
            return nullptr;
        }

        // 서버 모드는 인증서 필수
        if (config.mode == TlsMode::SERVER && !has_certificate) {
            std::cerr << "[TLS] Server mode requires certificate" << std::endl;
            return nullptr;
        }

        // CA 필수
        if (!has_ca) {
            std::cerr << "[TLS] Peer verification requires CA" << std::endl;
            return nullptr;
        }

        SSL* ssl = SSL_new(ctx);
        if (!ssl) {
            std::cerr << "[TLS] Failed to create SSL: " << GetLastError() << std::endl;
            return nullptr;
        }

        return ssl;
    }

    std::string TlsContext::GetLastError() 
    {
        char buf[256];
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        return std::string(buf);
    }

    void TlsContext::GlobalInit() 
    {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    }

    void TlsContext::GlobalCleanup() 
    {
        EVP_cleanup();
        ERR_free_strings();
    }

    bool TlsContext::InitializeClientContext() 
    {
        const SSL_METHOD* method = TLS_client_method();
        ctx = SSL_CTX_new(method);
        
        if (!ctx) {
            std::cerr << "[TLS] Failed to create client context: " 
                      << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool TlsContext::InitializeServerContext() 
    {
        const SSL_METHOD* method = TLS_server_method();
        ctx = SSL_CTX_new(method);
        
        if (!ctx) {
            std::cerr << "[TLS] Failed to create server context: " 
                      << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool TlsContext::LoadCertificateFromMemory(const std::string& cert_pem) 
    {
        BIO* bio = BIO_new_mem_buf(cert_pem.data(), cert_pem.size());
        if (!bio) {
            std::cerr << "[TLS] Failed to create BIO for certificate" << std::endl;
            return false;
        }

        X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        if (!cert) {
            std::cerr << "[TLS] Failed to parse certificate: " 
                      << GetLastError() << std::endl;
            return false;
        }

        int result = SSL_CTX_use_certificate(ctx, cert);
        X509_free(cert);

        if (result != 1) {
            std::cerr << "[TLS] Failed to use certificate: " 
                      << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool TlsContext::LoadPrivateKeyFromMemory(const std::string& key_pem) 
    {
        BIO* bio = BIO_new_mem_buf(key_pem.data(), key_pem.size());
        if (!bio) {
            std::cerr << "[TLS] Failed to create BIO for private key" << std::endl;
            return false;
        }

        EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        if (!pkey) {
            std::cerr << "[TLS] Failed to parse private key: " 
                      << GetLastError() << std::endl;
            return false;
        }

        int result = SSL_CTX_use_PrivateKey(ctx, pkey);
        EVP_PKEY_free(pkey);

        if (result != 1) {
            std::cerr << "[TLS] Failed to use private key: " 
                      << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool TlsContext::LoadCAFromMemory(const std::string& ca_pem) 
    {
        BIO* bio = BIO_new_mem_buf(ca_pem.data(), ca_pem.size());
        if (!bio) {
            std::cerr << "[TLS] Failed to create BIO for CA" << std::endl;
            return false;
        }

        X509_STORE* store = SSL_CTX_get_cert_store(ctx);
        if (!store) {
            BIO_free(bio);
            std::cerr << "[TLS] Failed to get certificate store" << std::endl;
            return false;
        }

        X509* ca_cert = nullptr;
        int count = 0;
        
        while ((ca_cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr)) != nullptr) {
            if (X509_STORE_add_cert(store, ca_cert) == 1) {
                count++;
            }
            X509_free(ca_cert);
        }

        BIO_free(bio);

        if (count == 0) {
            std::cerr << "[TLS] No CA certificates loaded" << std::endl;
            return false;
        }
        return true;
    }

    bool TlsContext::SetCipherList() 
    {
        if (!config.cipher_list.empty()) {
            if (SSL_CTX_set_cipher_list(ctx, config.cipher_list.c_str()) != 1) {
                std::cerr << "[TLS] Failed to set cipher list: " 
                          << GetLastError() << std::endl;
                return false;
            }
            std::cout << "[TLS] Cipher list set (TLS 1.2)" << std::endl;
        }

        if (!config.cipher_suites.empty()) {
            if (SSL_CTX_set_ciphersuites(ctx, config.cipher_suites.c_str()) != 1) {
                std::cerr << "[TLS] Failed to set cipher suites: " 
                          << GetLastError() << std::endl;
                return false;
            }
            std::cout << "[TLS] Cipher suites set (TLS 1.3)" << std::endl;
        }

        return true;
    }

    bool TlsContext::SetTlsVersion() 
    {
        int min_version = (config.min_version == TlsVersion::TLS_1_3) 
                          ? TLS1_3_VERSION : TLS1_2_VERSION;

        if (SSL_CTX_set_min_proto_version(ctx, min_version) != 1) {
            std::cerr << "[TLS] Failed to set min TLS version: " 
                      << GetLastError() << std::endl;
            return false;
        }
       return true;
    }

    bool TlsContext::ConfigureVerification() 
    {
        int mode = (config.mode == TlsMode::SERVER) 
            ? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT  // 서버: 클라이언트 인증서 필수
            : SSL_VERIFY_PEER;                                   // 클라이언트: 서버 인증서 검증

        SSL_CTX_set_verify(ctx, mode, VerifyCallback);
        SSL_CTX_set_verify_depth(ctx, config.verify_depth);
        return true;
    }

    bool TlsContext::ConfigureSessionCache() 
    {
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
        return true;
    }

    int TlsContext::VerifyCallback(int preverify_ok, X509_STORE_CTX* store_ctx) 
    {
        if (!preverify_ok) {
            char buf[256];
            X509* err_cert = X509_STORE_CTX_get_current_cert(store_ctx);
            int err = X509_STORE_CTX_get_error(store_ctx);
            int depth = X509_STORE_CTX_get_error_depth(store_ctx);

            X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);

            std::cerr << "[TLS] Certificate verification failed:" << std::endl;
            std::cerr << "  Subject: " << buf << std::endl;
            std::cerr << "  Error: " << X509_verify_cert_error_string(err) << std::endl;
            std::cerr << "  Depth: " << depth << std::endl;
        }

        return preverify_ok;
    }

} // namespace mpc_engine::network::tls