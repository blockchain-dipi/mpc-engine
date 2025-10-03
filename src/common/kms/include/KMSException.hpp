// src/common/kms/include/KMSException.hpp
#pragma once
#include <stdexcept>
#include <string>

namespace mpc_engine::kms
{
    /**
     * @brief KMS 기본 예외 클래스
     */
    class KMSException : public std::runtime_error 
    {
    public:
        explicit KMSException(const std::string& msg) 
            : std::runtime_error(msg) {}
    };
    
    /**
     * @brief 비밀 값을 찾을 수 없을 때 발생
     */
    class SecretNotFoundException : public KMSException 
    {
    public:
        explicit SecretNotFoundException(const std::string& key_id) 
            : KMSException("Secret not found: " + key_id) {}
    };
    
    /**
     * @brief KMS 연결 실패 시 발생
     */
    class KMSConnectionException : public KMSException 
    {
    public:
        explicit KMSConnectionException(const std::string& msg) 
            : KMSException("KMS connection error: " + msg) {}
    };
    
    /**
     * @brief KMS 인증 실패 시 발생
     */
    class KMSAuthenticationException : public KMSException 
    {
    public:
        explicit KMSAuthenticationException(const std::string& msg) 
            : KMSException("KMS authentication error: " + msg) {}
    };
    
    /**
     * @brief 잘못된 KMS 설정 시 발생
     */
    class KMSConfigurationException : public KMSException 
    {
    public:
        explicit KMSConfigurationException(const std::string& msg) 
            : KMSException("KMS configuration error: " + msg) {}
    };
}