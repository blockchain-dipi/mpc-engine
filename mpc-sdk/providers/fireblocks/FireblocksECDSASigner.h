// providers/fireblocks/FireblocksECDSASigner.h
#pragma once

#include "IECDSASigner.h"

namespace mpc::providers::fireblocks {

    class FireblocksECDSASigner : public mpc::crypto::IECDSASigner {
    public:
        FireblocksECDSASigner();
        ~FireblocksECDSASigner() override;

        mpc::crypto::ECDSAMtaRequest Phase1_StartSigning(
            const std::string& key_id,
            const std::string& tx_id,
            const std::vector<uint8_t>& message_hash,
            const std::vector<uint64_t>& player_ids
        ) override;

        // ✅ 수정: Phase2_MtaResponse (GenerateMtaResponse ❌)
        mpc::crypto::ECDSAMtaResponse Phase2_MtaResponse(
            const std::string& tx_id,
            const std::map<uint64_t, mpc::crypto::ECDSAMtaRequest>& all_mta_requests
        ) override;

        // ✅ 수정: Phase3_MtaVerify (VerifyMta ❌)
        mpc::crypto::ECDSAMtaDelta Phase3_MtaVerify(
            const std::string& tx_id,
            const std::map<uint64_t, mpc::crypto::ECDSAMtaResponse>& all_mta_responses
        ) override;

        mpc::crypto::ECDSAPartialSignature Phase4_GetPartialSignature(
            const std::string& tx_id,
            const std::map<uint64_t, mpc::crypto::ECDSAMtaDelta>& all_deltas
        ) override;

        mpc::crypto::ECDSASignature Phase5_GetFinalSignature(
            const std::string& tx_id,
            const std::map<uint64_t, mpc::crypto::ECDSAPartialSignature>& all_partial_sigs
        ) override;
    };

} // namespace mpc::providers::fireblocks
