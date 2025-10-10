// src/common/resource/include/AzureReadOnlyResLoader.hpp
#pragma once

#include "IReadOnlyResLoader.hpp"

namespace mpc_engine::resource
{
    /**
     * @brief Azure Confidential Computing 리소스 로더
     * 
     * TPM 암호화된 Managed Disk 사용.
     * 읽기 전용으로 사용 (쓰기 가능하지만 이 인터페이스에서는 읽기만).
     */
    class AzureReadOnlyResLoader : public IReadOnlyResLoader
    {
    public:
        AzureReadOnlyResLoader();
        ~AzureReadOnlyResLoader() override = default;

        std::string ReadFile(const std::string& path) override;
        std::vector<uint8_t> ReadBinaryFile(const std::string& path) override;
        bool Exists(const std::string& path) override;
        bool IsInitialized() const override;

    private:
        bool initialized_;
    };

} // namespace mpc_engine::resource