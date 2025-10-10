// src/common/resource/src/IBMReadOnlyResLoader.cpp
#include "common/resource/include/IBMReadOnlyResLoader.hpp"
#include <stdexcept>

namespace mpc_engine::resource
{
    IBMReadOnlyResLoader::IBMReadOnlyResLoader()
        : initialized_(false)
    {
        // TODO: IBM Secure Execution 초기화
        // TODO: Root/Data 디스크 확인
    }

    std::string IBMReadOnlyResLoader::ReadFile(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    std::vector<uint8_t> IBMReadOnlyResLoader::ReadBinaryFile(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    bool IBMReadOnlyResLoader::Exists(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    bool IBMReadOnlyResLoader::IsInitialized() const
    {
        return initialized_;
    }

} // namespace mpc_engine::resource