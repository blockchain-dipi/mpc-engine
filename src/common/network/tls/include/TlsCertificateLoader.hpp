// src/common/network/tls/include/TlsCertificateLoader.hpp
#pragma once

#include "TlsContext.hpp"
#include "common/kms/include/IKeyManagementService.hpp"
#include "common/config/ConfigManager.hpp"
#include <memory>
#include <string>

namespace mpc_engine::network::tls
{
    /**
     * @brief TLS 인증서 로더 (설정 기반)
     * 
     * ConfigManager를 통해 환경별 인증서 로드:
     * - Local: .env 파일 경로 사용 (TLS_DOCKER_*, TLS_KMS_*)
     * - Cloud: KMS 키 ID 사용 (향후 구현)
     */
    class TlsCertificateLoader 
    {
    private:
        std::shared_ptr<mpc_engine::kms::IKeyManagementService> kms_;
        
    public:
        explicit TlsCertificateLoader(std::shared_ptr<mpc_engine::kms::IKeyManagementService> kms);

        /**
         * @brief CA 인증서 로드
         * Local: TLS_DOCKER_CA 경로에서 파일 읽기
         * Cloud: KMS에서 로드 (향후 구현)
         */
        std::string LoadCaCertificate();

        /**
         * @brief Coordinator 인증서 로드
         */
        CertificateData LoadCoordinatorCertificate();

        /**
         * @brief Node 인증서 로드
         * @param node_index Node 인덱스 (0, 1, 2)
         */
        CertificateData LoadNodeCertificate(int node_index);

        /**
         * @brief 로더 상태 확인
         */
        bool IsHealthy();

        /**
         * @brief 현재 저장된 인증서 목록 출력 (디버깅용)
         */
        void PrintStatus();

    private:
        bool IsLocalPlatform();
        std::string ReadPemFile(const std::string& file_path);
        
        // Coordinator용
        std::string GetCoordinatorCertPath();
        std::string GetCoordinatorKeyPath();
        
        // Node용 (인덱스 기반)
        std::string GetNodeCertPath(int node_index);
        std::string GetNodeKeyPath(int node_index);
    };
}