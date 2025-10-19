// providers/fireblocks/FireblocksEdDSASigner.h
#pragma once

#include "IEdDSASigner.h"

namespace mpc::providers::fireblocks {

    class FireblocksEdDSASigner : public mpc::crypto::IEdDSASigner {
    public:
        FireblocksEdDSASigner();
        ~FireblocksEdDSASigner() override;

        mpc::crypto::EdDSACommitment Phase1_StartSigning(
            const std::string& key_id,
            const std::string& tx_id,
            const std::vector<uint8_t>& message,
            const std::vector<uint64_t>& player_ids
        ) override;

        mpc::crypto::EdDSAR Phase2_DecommitR(
            const std::string& tx_id,
            const std::map<uint64_t, mpc::crypto::EdDSACommitment>& all_commitments
        ) override;

        // ✅ 수정: EdDSARsAndCommitments 반환 (bool ❌)
        mpc::crypto::EdDSARsAndCommitments Phase3_BroadcastR(
            const std::string& tx_id,
            const std::map<uint64_t, mpc::crypto::EdDSAR>& all_Rs
        ) override;

        // ✅ 수정: EdDSARsAndCommitments 파라미터 추가
        mpc::crypto::EdDSAPartialSignature Phase4_GetPartialSignature(
            const std::string& tx_id,
            const mpc::crypto::EdDSARsAndCommitments& Rs_and_commitments
        ) override;

        mpc::crypto::EdDSASignature Phase5_GetFinalSignature(
            const std::string& tx_id,
            const std::map<uint64_t, mpc::crypto::EdDSAPartialSignature>& all_partial_sigs
        ) override;
    };

} // namespace mpc::providers::fireblocks
