// interface/IEdDSASigner.h
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mpc::crypto {
    /**
     * @brief EdDSA 서명 Phase별 데이터 구조
     */
    
    // Phase 1: Commitment
    struct EdDSACommitment {
        std::vector<uint8_t> data;  // R commitment (SHA256 hash)
    };
    
    // Phase 2: R 값 (Decommitment)
    struct EdDSAR {
        std::vector<uint8_t> R;     // 32 bytes - public nonce
    };
    
    // Phase 3: R 값들과 Commitments
    struct EdDSARsAndCommitments {
        std::map<uint64_t, EdDSAR> Rs;                      // 각 참여자의 R
        std::map<uint64_t, EdDSACommitment> commitments;    // 각 참여자의 commitment
    };
    
    // Phase 4: Partial Signature
    struct EdDSAPartialSignature {
        std::vector<uint8_t> s_i;   // 부분 서명 스칼라
    };
    
    // Phase 5: Final Signature
    struct EdDSASignature {
        std::vector<uint8_t> R;     // 32 bytes (public nonce)
        std::vector<uint8_t> s;     // 32 bytes (signature scalar)
        // ❌ v 없음! (ECDSA와의 차이점)
    };
    
    /**
     * @brief EdDSA 서명 인터페이스
     * 
     * EdDSA 서명은 5단계로 진행됩니다:
     * 1. Commitment 생성
     * 2. R 값 공개 (Decommit)
     * 3. R 값 브로드캐스트
     * 4. Partial Signature (s_i) 생성
     * 5. Final Signature (R, s) 조합
     * 
     * EdDSA는 Commitment/Decommitment 프로토콜을 사용합니다.
     * (ECDSA의 MTA 프로토콜과 다름!)
     */
    class IEdDSASigner {
    public:
        virtual ~IEdDSASigner() = default;
    
        /**
         * @brief Phase 1: Commitment 생성
         * 
         * @param key_id 키 ID
         * @param tx_id 트랜잭션 ID
         * @param message 서명할 메시지 (해시되지 않은 원본)
         * @param player_ids 참여자 ID 목록
         * @return 자신의 R Commitment
         */
        virtual EdDSACommitment Phase1_StartSigning(
            const std::string& key_id,
            const std::string& tx_id,
            const std::vector<uint8_t>& message,
            const std::vector<uint64_t>& player_ids
        ) = 0;
    
        /**
         * @brief Phase 2: R 값 공개 (Decommit)
         * 
         * @param tx_id 트랜잭션 ID
         * @param all_commitments 모든 참여자의 Commitments
         * @return 자신의 R 값
         */
        virtual EdDSAR Phase2_DecommitR(
            const std::string& tx_id,
            const std::map<uint64_t, EdDSACommitment>& all_commitments
        ) = 0;
    
        /**
         * @brief Phase 3: R 값 브로드캐스트 및 검증
         * 
         * @param tx_id 트랜잭션 ID
         * @param all_Rs 모든 참여자의 R 값들
         * @return R 값들과 Commitments (검증된 데이터)
         */
        virtual EdDSARsAndCommitments Phase3_BroadcastR(
            const std::string& tx_id,
            const std::map<uint64_t, EdDSAR>& all_Rs
        ) = 0;
    
        /**
         * @brief Phase 4: Partial Signature (s_i) 생성
         * 
         * @param tx_id 트랜잭션 ID
         * @param Rs_and_commitments 검증된 R 값들과 Commitments
         * @return 자신의 부분 서명 (s_i)
         */
        virtual EdDSAPartialSignature Phase4_GetPartialSignature(
            const std::string& tx_id,
            const EdDSARsAndCommitments& Rs_and_commitments
        ) = 0;
    
        /**
         * @brief Phase 5: Final Signature 생성
         * 
         * @param tx_id 트랜잭션 ID
         * @param all_partial_sigs 모든 참여자의 부분 서명들
         * @return 최종 EdDSA 서명 (R, s) - v 없음!
         */
        virtual EdDSASignature Phase5_GetFinalSignature(
            const std::string& tx_id,
            const std::map<uint64_t, EdDSAPartialSignature>& all_partial_sigs
        ) = 0;
    };
} // namespace mpc::crypto