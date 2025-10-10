// src/common/resource/src/GoogleReadOnlyResLoader.cpp
#include "common/resource/include/GoogleReadOnlyResLoader.hpp"
#include <stdexcept>

namespace mpc_engine::resource
{
    GoogleReadOnlyResLoader::GoogleReadOnlyResLoader()
        : initialized_(false)
    {
        // TODO: Google Confidential VM 초기화
        // TODO: Hyperdisk 마운트 확인
    }

    std::string GoogleReadOnlyResLoader::ReadFile(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    std::vector<uint8_t> GoogleReadOnlyResLoader::ReadBinaryFile(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    bool GoogleReadOnlyResLoader::Exists(const std::string& path)
    {
        throw std::runtime_error("Not implemented yet");
    }

    bool GoogleReadOnlyResLoader::IsInitialized() const
    {
        return initialized_;
    }

} // namespace mpc_engine::resource