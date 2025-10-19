// interface/IKeyGenerator.h
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mpc::crypto {
    /**
     * @brief 지원하는 암호화 알고리즘
     */
    enum class CryptoAlgorithm {
        ECDSA_SECP256K1,    // Bitcoin, Ethereum
        ECDSA_SECP256R1,    // NIST P-256
        ECDSA_STARK,        // StarkNet
        EDDSA_ED25519,      // Solana, Polkadot
        UNKNOWN
    };
    
    /**
     * @brief 알고리즘을 문자열로 변환 (내부 Provider와 통신용)
     */
    inline std::string CryptoAlgorithmToString(CryptoAlgorithm algo) {
        switch (algo) {
            case CryptoAlgorithm::ECDSA_SECP256K1: return "ECDSA_SECP256K1";
            case CryptoAlgorithm::ECDSA_SECP256R1: return "ECDSA_SECP256R1";
            case CryptoAlgorithm::ECDSA_STARK:     return "ECDSA_STARK";
            case CryptoAlgorithm::EDDSA_ED25519:   return "EDDSA_ED25519";
            default:                                return "UNKNOWN";
        }
    }
    
    /**
     * @brief 문자열을 알고리즘으로 변환 (내부 Provider 응답 파싱용)
     */
    inline CryptoAlgorithm StringToCryptoAlgorithm(const std::string& str) {
        if (str == "ECDSA_SECP256K1") return CryptoAlgorithm::ECDSA_SECP256K1;
        if (str == "ECDSA_SECP256R1") return CryptoAlgorithm::ECDSA_SECP256R1;
        if (str == "ECDSA_STARK")     return CryptoAlgorithm::ECDSA_STARK;
        if (str == "EDDSA_ED25519")   return CryptoAlgorithm::EDDSA_ED25519;
        return CryptoAlgorithm::UNKNOWN;
    }
    
    /**
     * @brief 키 생성 Phase별 데이터 구조
     */
    
    // Phase 1: Commitment
    struct KeyGenCommitment {
        std::vector<uint8_t> data;
    };
    
    // Phase 2: Decommitment
    struct KeyGenDecommitment {
        std::vector<uint8_t> data;
    };
    
    // Phase 3: ZK Proof
    struct KeyGenZKProof {
        std::vector<uint8_t> data;
    };
    
    // Phase 4: Paillier Proof
    struct KeyGenPaillierProof {
        std::vector<uint8_t> data;
    };
    
    // Phase 5: Public Key Result
    struct KeyGenResult {
        std::string public_key;           // 최종 공개키 (hex 또는 base64)
        CryptoAlgorithm algorithm;        // ✅ enum으로 타입 안전성 확보
        uint64_t player_id;               // 현재 플레이어 ID
    };
    
    /**
     * @brief 키 생성 인터페이스
     * 
     * MPC 키 생성은 5단계로 진행됩니다:
     * 1. Commitment 생성
     * 2. Decommitment 생성
     * 3. ZK Proof 생성
     * 4. Paillier Proof 검증
     * 5. 공개키 생성
     */
    class IKeyGenerator {
    public:
        virtual ~IKeyGenerator() = default;
    
        /**
         * @brief Phase 1: Commitment 생성
         * 
         * @param key_id 키 ID
         * @param tenant_id 테넌트 ID
         * @param algorithm 알고리즘 (enum)
         * @param player_ids 모든 참여자 ID 목록
         * @param threshold Threshold (예: 2/3의 경우 2)
         * @return 자신의 Commitment
         */
        virtual KeyGenCommitment Phase1_GenerateCommitment(
            const std::string& key_id,
            const std::string& tenant_id,
            CryptoAlgorithm algorithm,              // ✅ enum 사용
            const std::vector<uint64_t>& player_ids,
            uint32_t threshold
        ) = 0;
    
        /**
         * @brief Phase 2: Decommitment 생성
         * 
         * @param key_id 키 ID
         * @param all_commitments 모든 참여자의 Commitments
         * @return 자신의 Decommitment
         */
        virtual KeyGenDecommitment Phase2_GenerateDecommitment(
            const std::string& key_id,
            const std::map<uint64_t, KeyGenCommitment>& all_commitments
        ) = 0;
    
        /**
         * @brief Phase 3: ZK Proof 생성
         * 
         * @param key_id 키 ID
         * @param all_decommitments 모든 참여자의 Decommitments
         * @return 자신의 ZK Proof
         */
        virtual KeyGenZKProof Phase3_GenerateZKProof(
            const std::string& key_id,
            const std::map<uint64_t, KeyGenDecommitment>& all_decommitments
        ) = 0;
    
        /**
         * @brief Phase 4: Paillier Proof 검증 및 생성
         * 
         * @param key_id 키 ID
         * @param all_zk_proofs 모든 참여자의 ZK Proofs
         * @return 자신의 Paillier Proof
         */
        virtual KeyGenPaillierProof Phase4_VerifyAndGeneratePaillierProof(
            const std::string& key_id,
            const std::map<uint64_t, KeyGenZKProof>& all_zk_proofs
        ) = 0;
    
        /**
         * @brief Phase 5: 공개키 생성
         * 
         * @param key_id 키 ID
         * @param all_paillier_proofs 모든 참여자의 Paillier Proofs
         * @return 최종 공개키 및 알고리즘 정보
         */
        virtual KeyGenResult Phase5_CreatePublicKey(
            const std::string& key_id,
            const std::map<uint64_t, KeyGenPaillierProof>& all_paillier_proofs
        ) = 0;
    };
} // namespace mpc::crypto