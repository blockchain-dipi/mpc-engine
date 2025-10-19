// providers/fireblocks/FireblocksKeyGenerator.h
#pragma once

#include "IKeyGenerator.h"

namespace mpc::providers::fireblocks {

    class FireblocksKeyGenerator : public mpc::crypto::IKeyGenerator {
    public:
        FireblocksKeyGenerator();
        ~FireblocksKeyGenerator() override;

        mpc::crypto::KeyGenCommitment Phase1_GenerateCommitment(
            const std::string& key_id,
            const std::string& tenant_id,
            mpc::crypto::CryptoAlgorithm algorithm,
            const std::vector<uint64_t>& player_ids,
            uint32_t threshold
        ) override;

        mpc::crypto::KeyGenDecommitment Phase2_GenerateDecommitment(
            const std::string& key_id,
            const std::map<uint64_t, mpc::crypto::KeyGenCommitment>& all_commitments
        ) override;

        mpc::crypto::KeyGenZKProof Phase3_GenerateZKProof(
            const std::string& key_id,
            const std::map<uint64_t, mpc::crypto::KeyGenDecommitment>& all_decommitments
        ) override;

        mpc::crypto::KeyGenPaillierProof Phase4_VerifyAndGeneratePaillierProof(
            const std::string& key_id,
            const std::map<uint64_t, mpc::crypto::KeyGenZKProof>& all_zk_proofs
        ) override;

        mpc::crypto::KeyGenResult Phase5_CreatePublicKey(
            const std::string& key_id,
            const std::map<uint64_t, mpc::crypto::KeyGenPaillierProof>& all_paillier_proofs
        ) override;
    };

} // namespace mpc::providers::fireblocks
