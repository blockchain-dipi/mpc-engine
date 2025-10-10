// src/common/resource/src/AWSReadOnlyResLoader.cpp
#include "common/resource/include/AWSReadOnlyResLoader.hpp"
#include <stdexcept>

namespace mpc_engine::resource
{
    AWSReadOnlyResLoader::AWSReadOnlyResLoader()
        : initialized_(false)
    {
        // TODO: AWS Nitro Enclaves vsock 초기화
        // TODO: 부모 EC2로부터 리소스 수신 및 메모리 캐싱
    }

    std::string AWSReadOnlyResLoader::ReadFile(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    std::vector<uint8_t> AWSReadOnlyResLoader::ReadBinaryFile(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    bool AWSReadOnlyResLoader::Exists(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    bool AWSReadOnlyResLoader::IsInitialized() const
    {
        return initialized_;
    }

} // namespace mpc_engine::resource