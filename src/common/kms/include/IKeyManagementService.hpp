// src/common/kms/include/IKeyManagementService.hpp
#pragma once
#include <string>
#include <memory>

namespace mpc_engine::kms
{
    /**
     * @brief KMS (Key Management Service) 공통 인터페이스
     * 
     * 모든 KMS 구현체(Local, AWS, Azure, IBM)가 상속받아야 하는 순수 가상 인터페이스
     * 클라우드 중립적인 키 관리 API 제공
     */
    class IKeyManagementService 
    {
    public:
        virtual ~IKeyManagementService() = default;
        
        /**
         * @brief KMS 초기화
         * @return 성공 시 true, 실패 시 false
         */
        virtual bool Initialize() = 0;
        
        /**
         * @brief 비밀 값 조회
         * @param key_id 조회할 키 ID
         * @return 비밀 값 (평문)
         * @throws SecretNotFoundException 키가 존재하지 않을 때
         * @throws KMSException 기타 KMS 에러
         */
        virtual std::string GetSecret(const std::string& key_id) const = 0;
        
        /**
         * @brief 비밀 값 저장
         * @param key_id 저장할 키 ID
         * @param value 저장할 값 (평문)
         * @return 성공 시 true, 실패 시 false
         * @throws KMSException KMS 에러
         */
        virtual bool PutSecret(const std::string& key_id, const std::string& value) = 0;
        
        /**
         * @brief 비밀 값 삭제
         * @param key_id 삭제할 키 ID
         * @return 성공 시 true, 존재하지 않거나 실패 시 false
         */
        virtual bool DeleteSecret(const std::string& key_id) = 0;
        
        /**
         * @brief 비밀 값 존재 여부 확인
         * @param key_id 확인할 키 ID
         * @return 존재 시 true, 없으면 false
         */
        virtual bool SecretExists(const std::string& key_id) const = 0;
        
        /**
         * @brief KMS 초기화 상태 확인
         * @return 초기화됨 true, 아니면 false
         */
        virtual bool IsInitialized() const = 0;
    };
}