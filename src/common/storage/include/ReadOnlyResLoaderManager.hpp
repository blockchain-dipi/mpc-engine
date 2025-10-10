// src/common/resource/include/ReadOnlyResLoaderManager.hpp
#pragma once

#include "IReadOnlyResLoader.hpp"
#include "common/types/BasicTypes.hpp"
#include <memory>
#include <string>

namespace mpc_engine::resource
{
    /**
     * @brief 읽기 전용 리소스 로더 매니저
     * 
     * 플랫폼에 따라 적절한 ResourceLoader 구현체를 생성하고 관리.
     * Singleton 패턴으로 전역 접근 제공.
     * 
     * 사용 예시:
     * ```cpp
     * auto& manager = ReadOnlyResLoaderManager::Instance();
     * manager.Initialize(env::PlatformType::AWS);
     * 
     * std::string config = manager.ReadFile("/config/node.json");
     * auto key = manager.ReadBinaryFile("/keys/private.key");
     * ```
     */
    class ReadOnlyResLoaderManager
    {
    public:
        // Singleton 인스턴스 접근
        static ReadOnlyResLoaderManager& Instance();

        // 복사/이동 방지
        ReadOnlyResLoaderManager(const ReadOnlyResLoaderManager&) = delete;
        ReadOnlyResLoaderManager& operator=(const ReadOnlyResLoaderManager&) = delete;
        ReadOnlyResLoaderManager(ReadOnlyResLoaderManager&&) = delete;
        ReadOnlyResLoaderManager& operator=(ReadOnlyResLoaderManager&&) = delete;

        /**
         * @brief 리소스 로더 초기화
         * @param platform_type 플랫폼 타입
         * @throws std::runtime_error 초기화 실패 시
         * 
         * 플랫폼에 따라 적절한 구현체를 생성:
         * - LOCAL → LocalReadOnlyResLoader
         * - AWS → AWSReadOnlyResLoader
         * - AZURE → AzureReadOnlyResLoader
         * - IBM → IBMReadOnlyResLoader
         * - GOOGLE → GoogleReadOnlyResLoader
         */
        void Initialize(PlatformType platform_type);

        /**
         * @brief 텍스트 리소스 읽기
         * @param path 리소스 경로
         * @return 리소스 내용
         * @throws std::runtime_error 초기화되지 않았거나 읽기 실패 시
         */
        std::string ReadFile(const std::string& path);

        /**
         * @brief 바이너리 리소스 읽기
         * @param path 리소스 경로
         * @return 바이너리 데이터
         * @throws std::runtime_error 초기화되지 않았거나 읽기 실패 시
         */
        std::vector<uint8_t> ReadBinaryFile(const std::string& path);

        /**
         * @brief 리소스 존재 확인
         * @param path 리소스 경로
         * @return 리소스 존재 여부
         * @throws std::runtime_error 초기화되지 않은 경우
         */
        bool Exists(const std::string& path);

        /**
         * @brief 초기화 여부 확인
         * @return 초기화 완료 시 true
         */
        bool IsInitialized() const;

        /**
         * @brief 현재 플랫폼 타입 반환
         * @return 플랫폼 타입 (LOCAL, AWS, AZURE, IBM, GOOGLE)
         * @throws std::runtime_error 초기화되지 않은 경우
         */
        PlatformType GetPlatformType() const;

    private:
        ReadOnlyResLoaderManager() = default;
        ~ReadOnlyResLoaderManager() = default;

        std::unique_ptr<IReadOnlyResLoader> loader_;
        PlatformType platform_type_ = PlatformType::LOCAL;
        bool initialized_ = false;
    };

} // namespace mpc_engine::resource