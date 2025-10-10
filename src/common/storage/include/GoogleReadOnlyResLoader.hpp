// src/common/resource/include/GoogleReadOnlyResLoader.hpp
#pragma once

#include "IReadOnlyResLoader.hpp"

namespace mpc_engine::resource
{
    /**
     * @brief Google Confidential VM 리소스 로더
     * 
     * Hyperdisk Balanced (Confidential mode) 사용.
     * 읽기 전용으로 사용 (쓰기 가능하지만 이 인터페이스에서는 읽기만).
     */
    class GoogleReadOnlyResLoader : public IReadOnlyResLoader
    {
    public:
        GoogleReadOnlyResLoader();
        ~GoogleReadOnlyResLoader() override = default;

        std::string ReadFile(const std::string& path) override;
        std::vector<uint8_t> ReadBinaryFile(const std::string& path) override;
        bool Exists(const std::string& path) override;
        bool IsInitialized() const override;

    private:
        bool initialized_;
    };

} // namespace mpc_engine::resource