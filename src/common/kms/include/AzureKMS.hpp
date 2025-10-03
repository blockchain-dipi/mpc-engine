// src/common/kms/include/AzureKMS.hpp
#pragma once
#include "common/kms/include/IKeyManagementService.hpp"

namespace mpc_engine::kms
{
    /**
     * @brief Azure Key Vault 구현 (스텁)
     * 
     * Phase 9에서 Azure SDK 연동 예정
     * 현재는 로그만 출력
     */
    class AzureKMS : public IKeyManagementService 
    {
    public:
        AzureKMS() = default;
        ~AzureKMS() override = default;

        bool Initialize() override;
        std::string GetSecret(const std::string& key_id) const override;
        bool PutSecret(const std::string& key_id, const std::string& value) override;
        bool DeleteSecret(const std::string& key_id) override;
        bool SecretExists(const std::string& key_id) const override;
        bool IsInitialized() const override { return false; }
    };
}