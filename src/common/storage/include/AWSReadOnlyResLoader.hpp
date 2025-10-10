// src/common/resource/include/AWSReadOnlyResLoader.hpp
#pragma once

#include "IReadOnlyResLoader.hpp"

namespace mpc_engine::resource
{
    /**
     * @brief AWS Nitro Enclaves 리소스 로더
     * 
     * 메모리 전용 스토리지 (vsock으로 데이터 수신).
     * Enclave 시작 시 부모 EC2로부터 리소스를 메모리에 로드.
     */
    class AWSReadOnlyResLoader : public IReadOnlyResLoader
    {
    public:
        AWSReadOnlyResLoader();
        ~AWSReadOnlyResLoader() override = default;

        std::string ReadFile(const std::string& path) override;
        std::vector<uint8_t> ReadBinaryFile(const std::string& path) override;
        bool Exists(const std::string& path) override;
        bool IsInitialized() const override;

    private:
        bool initialized_;
    };

} // namespace mpc_engine::resource