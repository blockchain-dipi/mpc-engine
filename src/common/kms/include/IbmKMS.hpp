// src/common/kms/include/IbmKMS.hpp
#pragma once
#include "common/kms/include/IKeyManagementService.hpp"

namespace mpc_engine::kms
{
    /**
     * @brief IBM Secrets Manager 구현 (스텁)
     * 
     * Phase 9에서 IBM Cloud SDK 연동 예정
     * 현재는 로그만 출력
     */
    class IbmKMS : public IKeyManagementService 
    {
    public:
        IbmKMS() = default;
        ~IbmKMS() override = default;

        bool Initialize() override;
        std::string GetSecret(const std::string& key_id) const override;
        bool PutSecret(const std::string& key_id, const std::string& value) override;
        bool DeleteSecret(const std::string& key_id) override;
        bool SecretExists(const std::string& key_id) const override;
        bool IsInitialized() const override { return false; }
    };
}