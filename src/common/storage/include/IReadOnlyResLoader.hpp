// src/common/resource/include/IReadOnlyResLoader.hpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace mpc_engine::resource
{
    /**
     * @brief 읽기 전용 리소스 로더 인터페이스
     * 
     * 클라우드 플랫폼에 관계없이 리소스 파일을 읽기 위한 추상 인터페이스.
     * 모든 구현체는 읽기 전용이며, 쓰기/삭제 기능은 제공하지 않음.
     * 
     * 플랫폼별 특징:
     * - LOCAL: 로컬 파일 시스템 직접 접근
     * - AWS: 메모리 전용 (vsock으로 수신한 데이터)
     * - Azure: TPM 암호화된 Managed Disk
     * - IBM: Root 디스크(RO) + Data 디스크(RW) - 읽기만 사용
     * - Google: Hyperdisk Balanced (Confidential mode)
     * 
     * 사용 예시:
     * ```cpp
     * auto& manager = ReadOnlyResLoaderManager::Instance();
     * manager.Initialize(PlatformType::AWS);
     * 
     * std::string config = manager.ReadFile("/config/node.json");
     * auto key = manager.ReadBinaryFile("/keys/private.key");
     * ```
     */
    class IReadOnlyResLoader
    {
    public:
        virtual ~IReadOnlyResLoader() = default;

        /**
         * @brief 텍스트 리소스 읽기
         * @param path 리소스 경로 (절대 경로 또는 상대 경로)
         * @return 리소스 내용 (UTF-8 문자열)
         * @throws std::runtime_error 리소스가 없거나 읽기 실패 시
         * 
         * 용도: JSON, YAML, 설정 파일 등
         */
        virtual std::string ReadFile(const std::string& path) = 0;

        /**
         * @brief 바이너리 리소스 읽기
         * @param path 리소스 경로
         * @return 바이너리 데이터 (uint8_t 벡터)
         * @throws std::runtime_error 리소스가 없거나 읽기 실패 시
         * 
         * 용도: 키 파일, 인증서, 암호화된 데이터 등
         */
        virtual std::vector<uint8_t> ReadBinaryFile(const std::string& path) = 0;

        /**
         * @brief 리소스 존재 확인
         * @param path 리소스 경로
         * @return 리소스가 존재하면 true
         */
        virtual bool Exists(const std::string& path) = 0;

        /**
         * @brief 초기화 여부 확인
         * @return 초기화 완료 시 true
         */
        virtual bool IsInitialized() const = 0;
    };

} // namespace mpc_engine::resource