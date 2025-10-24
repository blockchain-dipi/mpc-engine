// providers/fireblocks/FireblocksCryptoProvider.cpp
#include "FireblocksCryptoProvider.h"
#include "FireblocksKeyGenerator.h"
#include "FireblocksECDSASigner.h"
#include "FireblocksEdDSASigner.h"

namespace mpc::providers::fireblocks {

FireblocksCryptoProvider::FireblocksCryptoProvider(
    uint64_t player_id,
    const std::string& tenant_id
)
    : key_generator_(std::make_unique<FireblocksKeyGenerator>(player_id, tenant_id))
    , ecdsa_signer_(std::make_unique<FireblocksECDSASigner>())
    , eddsa_signer_(std::make_unique<FireblocksEdDSASigner>())
{
}

FireblocksCryptoProvider::~FireblocksCryptoProvider() = default;

mpc::crypto::IKeyGenerator* FireblocksCryptoProvider::GetKeyGenerator() {
    return key_generator_.get();
}

mpc::crypto::IECDSASigner* FireblocksCryptoProvider::GetECDSASigner() {
    return ecdsa_signer_.get();
}

mpc::crypto::IEdDSASigner* FireblocksCryptoProvider::GetEdDSASigner() {
    return eddsa_signer_.get();
}

std::string FireblocksCryptoProvider::GetProviderName() const {
    return "Fireblocks";
}

std::string FireblocksCryptoProvider::GetVersion() const {
    return "1.0.0";
}

} // namespace mpc::providers::fireblocks