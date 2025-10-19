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
        FireblocksCryptoProvider();
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
