// providers/fireblocks/FireblocksECDSASigner.cpp
#include "FireblocksECDSASigner.h"
#include <iostream>

namespace mpc::providers::fireblocks {

    FireblocksECDSASigner::FireblocksECDSASigner() {
        std::cout << "[FireblocksECDSASigner] Initialized" << std::endl;
    }

    FireblocksECDSASigner::~FireblocksECDSASigner() = default;

    mpc::crypto::ECDSAMtaRequest FireblocksECDSASigner::Phase1_StartSigning(
        const std::string& /*key_id*/,
        const std::string& /*tx_id*/,
        const std::vector<uint8_t>& /*message_hash*/,
        const std::vector<uint64_t>& /*player_ids*/
    ) {
        std::cout << "[FireblocksECDSASigner] Phase1 - TODO" << std::endl;
        mpc::crypto::ECDSAMtaRequest req;
        req.data.resize(128, 0x11);
        return req;
    }

    // ✅ 수정: Phase2_MtaResponse
    mpc::crypto::ECDSAMtaResponse FireblocksECDSASigner::Phase2_MtaResponse(
        const std::string& /*tx_id*/,
        const std::map<uint64_t, mpc::crypto::ECDSAMtaRequest>& /*all_mta_requests*/
    ) {
        std::cout << "[FireblocksECDSASigner] Phase2 - TODO" << std::endl;
        mpc::crypto::ECDSAMtaResponse resp;
        resp.data.resize(128, 0x22);
        return resp;
    }

    // ✅ 수정: Phase3_MtaVerify
    mpc::crypto::ECDSAMtaDelta FireblocksECDSASigner::Phase3_MtaVerify(
        const std::string& /*tx_id*/,
        const std::map<uint64_t, mpc::crypto::ECDSAMtaResponse>& /*all_mta_responses*/
    ) {
        std::cout << "[FireblocksECDSASigner] Phase3 - TODO" << std::endl;
        mpc::crypto::ECDSAMtaDelta delta;
        delta.data.resize(64, 0x33);
        return delta;
    }

    mpc::crypto::ECDSAPartialSignature FireblocksECDSASigner::Phase4_GetPartialSignature(
        const std::string& /*tx_id*/,
        const std::map<uint64_t, mpc::crypto::ECDSAMtaDelta>& /*all_deltas*/
    ) {
        std::cout << "[FireblocksECDSASigner] Phase4 - TODO" << std::endl;
        mpc::crypto::ECDSAPartialSignature sig;
        sig.s_i.resize(32, 0x44);
        return sig;
    }

    mpc::crypto::ECDSASignature FireblocksECDSASigner::Phase5_GetFinalSignature(
        const std::string& /*tx_id*/,
        const std::map<uint64_t, mpc::crypto::ECDSAPartialSignature>& /*all_partial_sigs*/
    ) {
        std::cout << "[FireblocksECDSASigner] Phase5 - TODO" << std::endl;
        mpc::crypto::ECDSASignature sig;
        sig.r.resize(32, 0x55);
        sig.s.resize(32, 0x66);
        sig.v = 0x1b;
        return sig;
    }

} // namespace mpc::providers::fireblocks
