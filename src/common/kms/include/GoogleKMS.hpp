// src/common/kms/include/GoogleKMS.hpp
#pragma once
#include "common/kms/include/IKeyManagementService.hpp"

namespace mpc_engine::kms
{
    /**
     * @brief Google Secret Manager 구현 (스텁)
     * 
     * Phase 9에서 Google Cloud SDK 연동 예정
     * 현재는 로그만 출력
     */
    class GoogleKMS : public IKeyManagementService 
    {
    public:
        GoogleKMS() = default;
        ~GoogleKMS() override = default;

        bool Initialize() override;
        std::string GetSecret(const std::string& key_id) const override;
        bool PutSecret(const std::string& key_id, const std::string& value) override;
        bool DeleteSecret(const std::string& key_id) override;
        bool SecretExists(const std::string& key_id) const override;
        bool IsInitialized() const override { return false; }
    };
}