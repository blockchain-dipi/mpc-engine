// providers/fireblocks/FireblocksCryptoProvider.h
#pragma once

#include "ICryptoProvider.h"
#include <memory>
#include <string>

namespace mpc::providers::fireblocks {

    class FireblocksKeyGenerator;
    class FireblocksECDSASigner;
    class FireblocksEdDSASigner;

    class FireblocksCryptoProvider : public mpc::crypto::ICryptoProvider {
    public:
        /**
         * @brief 생성자
         * @param player_id 플레이어 ID
         * @param tenant_id Tenant ID
         */
        explicit FireblocksCryptoProvider(
            uint64_t player_id,
            const std::string& tenant_id
        );
        
        ~FireblocksCryptoProvider() override;

        mpc::crypto::IKeyGenerator* GetKeyGenerator() override;
        mpc::crypto::IECDSASigner* GetECDSASigner() override;
        mpc::crypto::IEdDSASigner* GetEdDSASigner() override;
        
        std::string GetProviderName() const override;
        std::string GetVersion() const override;

    private:
        std::unique_ptr<FireblocksKeyGenerator> key_generator_;
        std::unique_ptr<FireblocksECDSASigner> ecdsa_signer_;
        std::unique_ptr<FireblocksEdDSASigner> eddsa_signer_;
    };

} // namespace mpc::providers::fireblocks