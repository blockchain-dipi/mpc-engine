// src/common/resource/include/IBMReadOnlyResLoader.hpp
#pragma once

#include "IReadOnlyResLoader.hpp"

namespace mpc_engine::resource
{
    /**
     * @brief IBM Secure Execution for Linux 리소스 로더
     * 
     * Root 디스크 (읽기 전용) + Data 디스크 (읽기/쓰기).
     * 이 인터페이스에서는 읽기만 사용.
     */
    class IBMReadOnlyResLoader : public IReadOnlyResLoader
    {
    public:
        IBMReadOnlyResLoader();
        ~IBMReadOnlyResLoader() override = default;

        std::string ReadFile(const std::string& path) override;
        std::vector<uint8_t> ReadBinaryFile(const std::string& path) override;
        bool Exists(const std::string& path) override;
        bool IsInitialized() const override;

    private:
        bool initialized_;
    };

} // namespace mpc_engine::resource