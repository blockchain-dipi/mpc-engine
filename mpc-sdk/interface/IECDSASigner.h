// interface/IECDSASigner.h
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mpc::crypto {
    /**
     * @brief ECDSA 서명 Phase별 데이터 구조
     */
    
    // Phase 1: MTA Request (Multiplicative-to-Additive)
    struct ECDSAMtaRequest {
        std::vector<uint8_t> data;
    };
    
    // Phase 2: MTA Response
    struct ECDSAMtaResponse {
        std::vector<uint8_t> data;
    };
    
    // Phase 3: MTA Delta (검증 결과)
    struct ECDSAMtaDelta {
        std::vector<uint8_t> data;
    };
    
    // Phase 4: Partial Signature (s_i)
    struct ECDSAPartialSignature {
        std::vector<uint8_t> s_i;  // 부분 서명 스칼라
    };
    
    // Phase 5: Final Signature
    struct ECDSASignature {
        std::vector<uint8_t> r;     // 32 bytes
        std::vector<uint8_t> s;     // 32 bytes
        uint8_t v;                  // recovery id (0 or 1) - ECDSA만 있음!
    };
    
    /**
     * @brief ECDSA 서명 인터페이스
     * 
     * ECDSA 서명은 5단계로 진행됩니다:
     * 1. MTA Request 생성
     * 2. MTA Response 생성
     * 3. MTA Delta 검증
     * 4. Partial Signature (s_i) 생성
     * 5. Final Signature (r, s, v) 조합
     * 
     * ECDSA는 MTA(Multiplicative-to-Additive) 프로토콜을 사용합니다.
     */
    class IECDSASigner {
    public:
        virtual ~IECDSASigner() = default;
    
        /**
         * @brief Phase 1: MTA Request 생성
         * 
         * @param key_id 키 ID
         * @param tx_id 트랜잭션 ID
         * @param message_hash 서명할 메시지 해시 (32 bytes)
         * @param player_ids 참여자 ID 목록
         * @return 자신의 MTA Request
         */
        virtual ECDSAMtaRequest Phase1_StartSigning(
            const std::string& key_id,
            const std::string& tx_id,
            const std::vector<uint8_t>& message_hash,
            const std::vector<uint64_t>& player_ids
        ) = 0;
    
        /**
         * @brief Phase 2: MTA Response 생성
         * 
         * @param tx_id 트랜잭션 ID
         * @param all_mta_requests 모든 참여자의 MTA Requests
         * @return 자신의 MTA Response
         */
        virtual ECDSAMtaResponse Phase2_MtaResponse(
            const std::string& tx_id,
            const std::map<uint64_t, ECDSAMtaRequest>& all_mta_requests
        ) = 0;
    
        /**
         * @brief Phase 3: MTA Delta 검증
         * 
         * @param tx_id 트랜잭션 ID
         * @param all_mta_responses 모든 참여자의 MTA Responses
         * @return 자신의 MTA Delta (검증 결과)
         */
        virtual ECDSAMtaDelta Phase3_MtaVerify(
            const std::string& tx_id,
            const std::map<uint64_t, ECDSAMtaResponse>& all_mta_responses
        ) = 0;
    
        /**
         * @brief Phase 4: Partial Signature (s_i) 생성
         * 
         * @param tx_id 트랜잭션 ID
         * @param all_deltas 모든 참여자의 MTA Deltas
         * @return 자신의 부분 서명 (s_i)
         */
        virtual ECDSAPartialSignature Phase4_GetPartialSignature(
            const std::string& tx_id,
            const std::map<uint64_t, ECDSAMtaDelta>& all_deltas
        ) = 0;
    
        /**
         * @brief Phase 5: Final Signature 생성
         * 
         * @param tx_id 트랜잭션 ID
         * @param all_partial_sigs 모든 참여자의 부분 서명들
         * @return 최종 ECDSA 서명 (r, s, v)
         */
        virtual ECDSASignature Phase5_GetFinalSignature(
            const std::string& tx_id,
            const std::map<uint64_t, ECDSAPartialSignature>& all_partial_sigs
        ) = 0;
    };
} // namespace mpc::crypto