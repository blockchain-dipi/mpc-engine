// src/common/kms/include/KMSFactory.hpp
#pragma once
#include "common/kms/include/IKeyManagementService.hpp"
#include <memory>
#include <string>
#include <vector>

namespace mpc_engine::kms
{
    /**
     * @brief KMS 인스턴스 생성을 위한 Factory 클래스
     * 
     * 설정에 따라 적절한 KMS 구현체를 생성
     * Singleton 패턴 사용
     */
    class KMSFactory 
    {
    public:
        /**
         * @brief KMS 인스턴스 생성
         * 
         * @param provider KMS 제공자 ("local", "aws", "azure", "ibm", "google")
         * @param config_path 설정 경로 (Local KMS의 경우 저장 디렉토리 경로)
         * @return KMS 인스턴스 (unique_ptr)
         * @throws std::invalid_argument 잘못된 provider
         */
        static std::unique_ptr<IKeyManagementService> Create(
            const std::string& provider,
            const std::string& config_path = ""
        );

        /**
         * @brief 지원하는 provider 목록 반환
         * 
         * @return 지원하는 provider 목록
         */
        static std::vector<std::string> GetSupportedProviders();

        /**
         * @brief Provider가 유효한지 확인
         * 
         * @param provider 확인할 provider 이름
         * @return 유효하면 true, 아니면 false
         */
        static bool IsValidProvider(const std::string& provider);

    private:
        KMSFactory() = delete;  // 인스턴스 생성 방지
        
        static std::string NormalizeProvider(const std::string& provider);
    };
}