// src/common/resource/include/LocalReadOnlyResLoader.hpp
#pragma once

#include "IReadOnlyResLoader.hpp"

namespace mpc_engine::resource
{
    /**
     * @brief 로컬 파일 시스템 리소스 로더
     * 
     * 표준 C++ filesystem 사용.
     * 개발/테스트 환경용.
     */
    class LocalReadOnlyResLoader : public IReadOnlyResLoader
    {
    public:
        LocalReadOnlyResLoader();
        ~LocalReadOnlyResLoader() override = default;

        std::string ReadFile(const std::string& path) override;
        std::vector<uint8_t> ReadBinaryFile(const std::string& path) override;
        bool Exists(const std::string& path) override;
        bool IsInitialized() const override;

    private:
        /**
         * @brief 경로 정규화 (상대 경로 처리)
         * @param path 원본 경로
         * @return 정규화된 절대 경로
         */
        std::string NormalizePath(const std::string& path) const;

        bool initialized_;
    };

} // namespace mpc_engine::resource