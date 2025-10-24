// mpc-sdk/providers/fireblocks/FireblocksKeyGenerator.cpp
#include "FireblocksKeyGenerator.h"
#include "MPCException.h"
#include <cosigner/cosigner_exception.h>
#include <iostream>

namespace mpc::providers::fireblocks {

using namespace ::fireblocks::common::cosigner;

// ========== Helper: setup_decommitment 직렬화 ==========
namespace {
    std::vector<uint8_t> SerializeDecommitment(const setup_decommitment& decommit) {
        std::vector<uint8_t> result;
        
        // ack (32 bytes)
        result.insert(result.end(), decommit.ack, decommit.ack + sizeof(decommit.ack));
        
        // seed (32 bytes)
        result.insert(result.end(), decommit.seed, decommit.seed + sizeof(decommit.seed));
        
        // share (65 bytes)
        result.insert(result.end(), 
            reinterpret_cast<const uint8_t*>(&decommit.share),
            reinterpret_cast<const uint8_t*>(&decommit.share) + sizeof(decommit.share));
        
        // paillier_public_key (size + data)
        uint32_t paillier_size = decommit.paillier_public_key.size();
        result.insert(result.end(), 
            reinterpret_cast<const uint8_t*>(&paillier_size),
            reinterpret_cast<const uint8_t*>(&paillier_size) + sizeof(paillier_size));
        result.insert(result.end(), 
            decommit.paillier_public_key.begin(), 
            decommit.paillier_public_key.end());
        
        // ring_pedersen_public_key (size + data)
        uint32_t ring_size = decommit.ring_pedersen_public_key.size();
        result.insert(result.end(), 
            reinterpret_cast<const uint8_t*>(&ring_size),
            reinterpret_cast<const uint8_t*>(&ring_size) + sizeof(ring_size));
        result.insert(result.end(), 
            decommit.ring_pedersen_public_key.begin(), 
            decommit.ring_pedersen_public_key.end());
        
        return result;
    }
    
    setup_decommitment DeserializeDecommitment(const std::vector<uint8_t>& data) {
        setup_decommitment result;
        size_t offset = 0;
        
        // ack
        std::copy(data.begin() + offset, data.begin() + offset + sizeof(result.ack), result.ack);
        offset += sizeof(result.ack);
        
        // seed
        std::copy(data.begin() + offset, data.begin() + offset + sizeof(result.seed), result.seed);
        offset += sizeof(result.seed);
        
        // share
        std::copy(data.begin() + offset, data.begin() + offset + sizeof(result.share),
            reinterpret_cast<uint8_t*>(&result.share));
        offset += sizeof(result.share);
        
        // paillier_public_key
        uint32_t paillier_size;
        std::copy(data.begin() + offset, data.begin() + offset + sizeof(paillier_size),
            reinterpret_cast<uint8_t*>(&paillier_size));
        offset += sizeof(paillier_size);
        result.paillier_public_key.assign(data.begin() + offset, data.begin() + offset + paillier_size);
        offset += paillier_size;
        
        // ring_pedersen_public_key
        uint32_t ring_size;
        std::copy(data.begin() + offset, data.begin() + offset + sizeof(ring_size),
            reinterpret_cast<uint8_t*>(&ring_size));
        offset += sizeof(ring_size);
        result.ring_pedersen_public_key.assign(data.begin() + offset, data.begin() + offset + ring_size);
        
        return result;
    }

    // Proofs 직렬화
    std::vector<uint8_t> SerializeZKProofs(const setup_zk_proofs& proofs) {
        std::vector<uint8_t> result;
        
        // schnorr_s (elliptic_curve_scalar - 32 bytes)
        result.insert(result.end(), 
            reinterpret_cast<const uint8_t*>(&proofs.schnorr_s),
            reinterpret_cast<const uint8_t*>(&proofs.schnorr_s) + sizeof(proofs.schnorr_s));
        
        // paillier_blum_zkp (size + data)
        uint32_t paillier_size = proofs.paillier_blum_zkp.size();
        result.insert(result.end(), 
            reinterpret_cast<const uint8_t*>(&paillier_size),
            reinterpret_cast<const uint8_t*>(&paillier_size) + sizeof(paillier_size));
        result.insert(result.end(), 
            proofs.paillier_blum_zkp.begin(), 
            proofs.paillier_blum_zkp.end());
        
        // ring_pedersen_param_zkp (size + data)
        uint32_t ring_size = proofs.ring_pedersen_param_zkp.size();
        result.insert(result.end(), 
            reinterpret_cast<const uint8_t*>(&ring_size),
            reinterpret_cast<const uint8_t*>(&ring_size) + sizeof(ring_size));
        result.insert(result.end(), 
            proofs.ring_pedersen_param_zkp.begin(), 
            proofs.ring_pedersen_param_zkp.end());
        
        return result;
    }
    
    setup_zk_proofs DeserializeZKProofs(const std::vector<uint8_t>& data) {
        setup_zk_proofs result;
        size_t offset = 0;
        
        // schnorr_s
        std::copy(data.begin() + offset, data.begin() + offset + sizeof(result.schnorr_s),
            reinterpret_cast<uint8_t*>(&result.schnorr_s));
        offset += sizeof(result.schnorr_s);
        
        // paillier_blum_zkp
        uint32_t paillier_size;
        std::copy(data.begin() + offset, data.begin() + offset + sizeof(paillier_size),
            reinterpret_cast<uint8_t*>(&paillier_size));
        offset += sizeof(paillier_size);
        result.paillier_blum_zkp.assign(data.begin() + offset, data.begin() + offset + paillier_size);
        offset += paillier_size;
        
        // ring_pedersen_param_zkp
        uint32_t ring_size;
        std::copy(data.begin() + offset, data.begin() + offset + sizeof(ring_size),
            reinterpret_cast<uint8_t*>(&ring_size));
        offset += sizeof(ring_size);
        result.ring_pedersen_param_zkp.assign(data.begin() + offset, data.begin() + offset + ring_size);
        
        return result;
    }
}

FireblocksKeyGenerator::FireblocksKeyGenerator(
    uint64_t player_id,
    const std::string& tenant_id
)
    : player_id_(player_id)
    , platform_(player_id, tenant_id)
    , persistency_()
    , service_(nullptr)
{
    service_ = std::make_unique<cmp_setup_service>(platform_, persistency_);
    std::cout << "[FireblocksKeyGenerator] Initialized for player " << player_id << std::endl;
}

FireblocksKeyGenerator::~FireblocksKeyGenerator() = default;

cosigner_sign_algorithm FireblocksKeyGenerator::ToFireblocksAlgorithm(
    mpc::crypto::CryptoAlgorithm algo
) const {
    switch (algo) {
        case mpc::crypto::CryptoAlgorithm::ECDSA_SECP256K1:
            return ECDSA_SECP256K1;
        case mpc::crypto::CryptoAlgorithm::ECDSA_SECP256R1:
            return ECDSA_SECP256R1;
        case mpc::crypto::CryptoAlgorithm::ECDSA_STARK:
            return ECDSA_STARK;
        case mpc::crypto::CryptoAlgorithm::EDDSA_ED25519:
            return EDDSA_ED25519;
        case mpc::crypto::CryptoAlgorithm::UNKNOWN:
            break;
    }
    throw mpc::MPCException(mpc::MPCErrorCode::INVALID_ALGORITHM, "Unsupported algorithm");
}

// ========== Phase 1: Commitment 생성 ==========
mpc::crypto::KeyGenCommitment FireblocksKeyGenerator::Phase1_GenerateCommitment(
    const std::string& key_id,
    const std::string& tenant_id,
    mpc::crypto::CryptoAlgorithm algorithm,
    const std::vector<uint64_t>& player_ids,
    uint32_t threshold
) {
    try {
        cosigner_sign_algorithm fb_algo = ToFireblocksAlgorithm(algorithm);
        
        commitment commit;
        service_->generate_setup_commitments(
            key_id,
            tenant_id,
            fb_algo,
            player_ids,
            threshold,
            0,
            {},
            commit
        );
        
        mpc::crypto::KeyGenCommitment result;
        const uint8_t* commit_bytes = reinterpret_cast<const uint8_t*>(&commit);
        result.data.assign(commit_bytes, commit_bytes + sizeof(commitment));
        
        std::cout << "[FireblocksKeyGenerator] Phase1 completed for key: " << key_id << std::endl;
        return result;
        
    } catch (const cosigner_exception& e) {
        throw mpc::MPCException(
            mpc::MPCErrorCode::KEY_GENERATION_FAILED,
            std::string("Phase1 failed: ") + std::to_string(e.error_code())
        );
    }
}

// ========== Phase 2: Decommitment 생성 ==========
mpc::crypto::KeyGenDecommitment FireblocksKeyGenerator::Phase2_GenerateDecommitment(
    const std::string& key_id,
    const std::map<uint64_t, mpc::crypto::KeyGenCommitment>& all_commitments
) {
    try {
        std::map<uint64_t, commitment> fb_commits;
        for (const auto& [pid, commit_data] : all_commitments) {
            if (commit_data.data.size() != sizeof(commitment)) {
                throw mpc::MPCException(
                    mpc::MPCErrorCode::INVALID_INPUT,
                    "Invalid commitment data size"
                );
            }
            
            commitment fb_commit;
            const uint8_t* src = commit_data.data.data();
            uint8_t* dst = reinterpret_cast<uint8_t*>(&fb_commit);
            std::copy(src, src + sizeof(commitment), dst);
            
            fb_commits[pid] = fb_commit;
        }
        
        setup_decommitment decommit;
        service_->store_setup_commitments(key_id, fb_commits, decommit);
        
        // ✅ 실제 decommitment를 직렬화
        mpc::crypto::KeyGenDecommitment result;
        result.data = SerializeDecommitment(decommit);
        
        std::cout << "[FireblocksKeyGenerator] Phase2 completed for key: " << key_id << std::endl;
        return result;
        
    } catch (const cosigner_exception& e) {
        throw mpc::MPCException(
            mpc::MPCErrorCode::KEY_GENERATION_FAILED,
            std::string("Phase2 failed: ") + std::to_string(e.error_code())
        );
    }
}

// ========== Phase 3: ZK Proof 생성 ==========
mpc::crypto::KeyGenZKProof FireblocksKeyGenerator::Phase3_GenerateZKProof(
    const std::string& key_id,
    const std::map<uint64_t, mpc::crypto::KeyGenDecommitment>& all_decommitments
) {
    try {
        std::map<uint64_t, setup_decommitment> fb_decommits;
        for (const auto& [pid, decommit_data] : all_decommitments) {
            fb_decommits[pid] = DeserializeDecommitment(decommit_data.data);
        }
        
        setup_zk_proofs proofs;
        service_->generate_setup_proofs(key_id, fb_decommits, proofs);
        
        // ✅ 수정: 실제 ZK proofs를 직렬화
        mpc::crypto::KeyGenZKProof result;
        result.data = SerializeZKProofs(proofs);
        
        std::cout << "[FireblocksKeyGenerator] Phase3 completed for key: " << key_id << std::endl;
        return result;
        
    } catch (const cosigner_exception& e) {
        throw mpc::MPCException(
            mpc::MPCErrorCode::KEY_GENERATION_FAILED,
            std::string("Phase3 failed: ") + std::to_string(e.error_code())
        );
    }
}

// ========== Phase 4: Paillier Proof 검증 및 생성 ==========
mpc::crypto::KeyGenPaillierProof FireblocksKeyGenerator::Phase4_VerifyAndGeneratePaillierProof(
    const std::string& key_id,
    const std::map<uint64_t, mpc::crypto::KeyGenZKProof>& all_zk_proofs
) {
    try {
        std::map<uint64_t, setup_zk_proofs> fb_proofs;
        for (const auto& [pid, proof_data] : all_zk_proofs) {
            fb_proofs[pid] = DeserializeZKProofs(proof_data.data);
        }
        
        // ✅ 수정: 이 player가 생성한 모든 proofs (다른 player들에게 보낼)
        std::map<uint64_t, byte_vector_t> paillier_proofs;
        service_->verify_setup_proofs(key_id, fb_proofs, paillier_proofs);
        
        // ✅ 수정: 전체 맵을 직렬화
        mpc::crypto::KeyGenPaillierProof result;
        
        // 직렬화: map<uint64_t, byte_vector_t>
        uint32_t map_size = paillier_proofs.size();
        result.data.insert(result.data.end(),
            reinterpret_cast<const uint8_t*>(&map_size),
            reinterpret_cast<const uint8_t*>(&map_size) + sizeof(map_size));
        
        for (const auto& [target_pid, proof_data] : paillier_proofs) {
            // player_id
            result.data.insert(result.data.end(),
                reinterpret_cast<const uint8_t*>(&target_pid),
                reinterpret_cast<const uint8_t*>(&target_pid) + sizeof(target_pid));
            
            // proof size
            uint32_t proof_size = proof_data.size();
            result.data.insert(result.data.end(),
                reinterpret_cast<const uint8_t*>(&proof_size),
                reinterpret_cast<const uint8_t*>(&proof_size) + sizeof(proof_size));
            
            // proof data
            result.data.insert(result.data.end(), proof_data.begin(), proof_data.end());
        }
        
        std::cout << "[FireblocksKeyGenerator] Phase4 completed for key: " << key_id << std::endl;
        return result;
        
    } catch (const cosigner_exception& e) {
        throw mpc::MPCException(
            mpc::MPCErrorCode::KEY_GENERATION_FAILED,
            std::string("Phase4 failed: ") + std::to_string(e.error_code())
        );
    }
}

// ========== Phase 5: 공개키 생성 ==========
mpc::crypto::KeyGenResult FireblocksKeyGenerator::Phase5_CreatePublicKey(
    const std::string& key_id,
    const std::map<uint64_t, mpc::crypto::KeyGenPaillierProof>& all_paillier_proofs
) {
    try {
        // ✅ 수정: 역직렬화
        std::map<uint64_t, std::map<uint64_t, byte_vector_t>> fb_proofs;
        
        for (const auto& [from_pid, serialized_proof] : all_paillier_proofs) {
            const auto& data = serialized_proof.data;
            size_t offset = 0;
            
            // map size
            uint32_t map_size;
            std::copy(data.begin() + offset, data.begin() + offset + sizeof(map_size),
                reinterpret_cast<uint8_t*>(&map_size));
            offset += sizeof(map_size);
            
            // 각 entry
            for (uint32_t i = 0; i < map_size; ++i) {
                // target player_id
                uint64_t target_pid;
                std::copy(data.begin() + offset, data.begin() + offset + sizeof(target_pid),
                    reinterpret_cast<uint8_t*>(&target_pid));
                offset += sizeof(target_pid);
                
                // proof size
                uint32_t proof_size;
                std::copy(data.begin() + offset, data.begin() + offset + sizeof(proof_size),
                    reinterpret_cast<uint8_t*>(&proof_size));
                offset += sizeof(proof_size);
                
                // proof data
                byte_vector_t proof_data(data.begin() + offset, data.begin() + offset + proof_size);
                offset += proof_size;
                
                fb_proofs[from_pid][target_pid] = proof_data;
            }
        }
        
        std::string public_key_str;
        cosigner_sign_algorithm algorithm;
        service_->create_secret(key_id, fb_proofs, public_key_str, algorithm);
        
        mpc::crypto::KeyGenResult result;
        result.public_key.assign(public_key_str.begin(), public_key_str.end());
        
        switch (algorithm) {
            case ECDSA_SECP256K1:
                result.algorithm = mpc::crypto::CryptoAlgorithm::ECDSA_SECP256K1;
                break;
            case ECDSA_SECP256R1:
                result.algorithm = mpc::crypto::CryptoAlgorithm::ECDSA_SECP256R1;
                break;
            case ECDSA_STARK:
                result.algorithm = mpc::crypto::CryptoAlgorithm::ECDSA_STARK;
                break;
            case EDDSA_ED25519:
                result.algorithm = mpc::crypto::CryptoAlgorithm::EDDSA_ED25519;
                break;
            default:
                result.algorithm = mpc::crypto::CryptoAlgorithm::ECDSA_SECP256K1;
        }
        
        std::cout << "[FireblocksKeyGenerator] Phase5 completed - Public key generated!" << std::endl;
        return result;
        
    } catch (const cosigner_exception& e) {
        throw mpc::MPCException(
            mpc::MPCErrorCode::KEY_GENERATION_FAILED,
            std::string("Phase5 failed: ") + std::to_string(e.error_code())
        );
    }
}

const FireblocksPersistency::KeyInfo* FireblocksKeyGenerator::GetGeneratedKey(
    const std::string& key_id
) const {
    return persistency_.get_key(key_id);
}

} // namespace mpc::providers::fireblocks