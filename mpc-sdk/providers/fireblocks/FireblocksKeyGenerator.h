// mpc-sdk/providers/fireblocks/FireblocksKeyGenerator.h
#pragma once

#include "IKeyGenerator.h"
#include "FireblocksPlatformService.h"
#include "FireblocksPersistency.h"
#include <cosigner/cmp_setup_service.h>
#include <memory>

namespace mpc::providers::fireblocks {

class FireblocksKeyGenerator : public mpc::crypto::IKeyGenerator {
public:
    /**
     * @brief 생성자
     * @param player_id 현재 플레이어 ID
     * @param tenant_id Tenant ID (기본값: "default-tenant")
     */
    explicit FireblocksKeyGenerator(
        uint64_t player_id,
        const std::string& tenant_id = "default-tenant"
    );
    
    ~FireblocksKeyGenerator() override;

    // ========== IKeyGenerator 인터페이스 구현 ==========

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

    // ========== Public API ==========
    
    /**
     * @brief 생성된 키 정보 조회 (Node가 DB 저장용)
     */
    const FireblocksPersistency::KeyInfo* GetGeneratedKey(const std::string& key_id) const;

private:
    // ========== Private 멤버 ==========
    
    uint64_t player_id_;
    FireblocksPlatformService platform_;
    FireblocksPersistency persistency_;
    std::unique_ptr<::fireblocks::common::cosigner::cmp_setup_service> service_;
    
    // Helper: CryptoAlgorithm → cosigner_sign_algorithm 변환
    cosigner_sign_algorithm ToFireblocksAlgorithm(mpc::crypto::CryptoAlgorithm algo) const;
};

} // namespace mpc::providers::fireblocks