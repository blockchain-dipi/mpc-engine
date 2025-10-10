// src/common/resource/src/AzureReadOnlyResLoader.cpp
#include "common/resource/include/AzureReadOnlyResLoader.hpp"
#include <stdexcept>

namespace mpc_engine::resource
{
    AzureReadOnlyResLoader::AzureReadOnlyResLoader()
        : initialized_(false)
    {
        // TODO: Azure Confidential Computing 초기화
        // TODO: TPM 암호화된 디스크 마운트 확인
    }

    std::string AzureReadOnlyResLoader::ReadFile(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    std::vector<uint8_t> AzureReadOnlyResLoader::ReadBinaryFile(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    bool AzureReadOnlyResLoader::Exists(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    bool AzureReadOnlyResLoader::IsInitialized() const
    {
        return initialized_;
    }

} // namespace mpc_engine::resource