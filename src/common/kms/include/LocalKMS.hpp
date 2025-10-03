// src/common/kms/include/LocalKMS.hpp
#pragma once

#include "IKeyManagementService.hpp"
#include <filesystem>
#include <mutex>

namespace mpc_engine::kms
{
    namespace fs = std::filesystem;

    class LocalKMS : public IKeyManagementService
    {
    private:
        fs::path storage_path;
        mutable std::mutex storage_mutex;
        bool is_initialized;

    public:
        explicit LocalKMS(const std::string& path);
        ~LocalKMS() override;

        bool Initialize() override;
        bool IsInitialized() const override;
        
        bool PutSecret(const std::string& key_id, const std::string& value) override;
        std::string GetSecret(const std::string& key_id) const override;
        bool SecretExists(const std::string& key_id) const override;
        bool DeleteSecret(const std::string& key_id) override;
    };

} // namespace mpc_engine::kms