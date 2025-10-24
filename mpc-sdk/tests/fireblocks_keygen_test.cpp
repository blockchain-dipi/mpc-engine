// mpc-sdk/tests/fireblocks_keygen_test.cpp
#include <gtest/gtest.h>
#include "IKeyGenerator.h"
#include "ICryptoProvider.h"
#include "FireblocksCryptoProvider.h"
#include "MPCException.h"
#include <iostream>
#include <iomanip>
#include <memory>

using namespace mpc::crypto;

// ========== Helper 함수 ==========

std::string BytesToHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : bytes) {
        ss << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
}

std::string BytesToHex(const std::string& str) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : str) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}

// ========== 테스트 Fixture ==========

class FireblocksKeyGenTest : public ::testing::Test {
protected:
    void SetUp() override {
        // ✅ Provider를 통해 생성 (player_id와 tenant_id 전달)
        provider1 = CreateProvider(1);
        provider2 = CreateProvider(2);
        provider3 = CreateProvider(3);
        
        // ✅ 인터페이스를 통해 KeyGenerator 획득
        node1 = provider1->GetKeyGenerator();
        node2 = provider2->GetKeyGenerator();
        node3 = provider3->GetKeyGenerator();
        
        player_ids = {1, 2, 3};
        threshold = 3;
        key_id = "test-key-001";
        tenant_id = "test-tenant";
        algorithm = CryptoAlgorithm::ECDSA_SECP256K1;
    }

    void TearDown() override {
        // Provider shared_ptr 자동 정리
    }
    
    /**
     * @brief Provider 팩토리 함수
     * ✅ 라이브러리 교체 시 여기만 수정!
     */
    std::shared_ptr<ICryptoProvider> CreateProvider(uint64_t player_id) {
        // ✅ player_id와 tenant_id 전달
        return std::make_shared<mpc::providers::fireblocks::FireblocksCryptoProvider>(
            player_id, 
            "test-tenant"  // ← 테스트에서 사용하는 tenant_id
        );
        
        // ZenGo로 교체하려면:
        // return std::make_shared<mpc::providers::zengo::ZenGoCryptoProvider>(player_id, "test-tenant");
    }

    // ✅ Provider (소유권)
    std::shared_ptr<ICryptoProvider> provider1;
    std::shared_ptr<ICryptoProvider> provider2;
    std::shared_ptr<ICryptoProvider> provider3;
    
    // ✅ 인터페이스 포인터
    IKeyGenerator* node1;
    IKeyGenerator* node2;
    IKeyGenerator* node3;
    
    std::vector<uint64_t> player_ids;
    uint32_t threshold;
    std::string key_id;
    std::string tenant_id;
    CryptoAlgorithm algorithm;
};

// ========== Test Case 1: Phase 1 - Commitment 생성 ==========

TEST_F(FireblocksKeyGenTest, Phase1_GenerateCommitment) {
    std::cout << "\n========== 3-of-3 Key Generation ==========" << std::endl;
    
    KeyGenCommitment commit1, commit2, commit3;
    
    ASSERT_NO_THROW({
        commit1 = node1->Phase1_GenerateCommitment(
            key_id, tenant_id, algorithm, player_ids, threshold
        );
    });
    
    ASSERT_NO_THROW({
        commit2 = node2->Phase1_GenerateCommitment(
            key_id, tenant_id, algorithm, player_ids, threshold
        );
    });
    
    ASSERT_NO_THROW({
        commit3 = node3->Phase1_GenerateCommitment(
            key_id, tenant_id, algorithm, player_ids, threshold
        );
    });
    
    EXPECT_GT(commit1.data.size(), 0);
    EXPECT_GT(commit2.data.size(), 0);
    EXPECT_GT(commit3.data.size(), 0);
    
    std::cout << "\n🎉 3-of-3 Key Generation SUCCESS!" << std::endl;
}

// ========== Test Case 2: Phase 1-5 전체 흐름 ==========

TEST_F(FireblocksKeyGenTest, Phase1to5_FullFlow_3of3) {
    std::cout << "\n========== Phase 1-5: Full Key Generation Flow ==========" << std::endl;
    
    // ===== Phase 1: Commitment =====
    std::cout << "\n--- Phase 1: Generate Commitments ---" << std::endl;
    
    KeyGenCommitment commit1 = node1->Phase1_GenerateCommitment(
        key_id, tenant_id, algorithm, player_ids, threshold
    );
    KeyGenCommitment commit2 = node2->Phase1_GenerateCommitment(
        key_id, tenant_id, algorithm, player_ids, threshold
    );
    KeyGenCommitment commit3 = node3->Phase1_GenerateCommitment(
        key_id, tenant_id, algorithm, player_ids, threshold
    );
    
    std::cout << "✅ All commitments generated" << std::endl;
    
    std::map<uint64_t, KeyGenCommitment> all_commitments = {
        {1, commit1},
        {2, commit2},
        {3, commit3}
    };
    
    // ===== Phase 2: Decommitment =====
    std::cout << "\n--- Phase 2: Generate Decommitments ---" << std::endl;
    
    KeyGenDecommitment decommit1 = node1->Phase2_GenerateDecommitment(key_id, all_commitments);
    KeyGenDecommitment decommit2 = node2->Phase2_GenerateDecommitment(key_id, all_commitments);
    KeyGenDecommitment decommit3 = node3->Phase2_GenerateDecommitment(key_id, all_commitments);
    
    std::cout << "✅ All decommitments generated" << std::endl;
    
    std::map<uint64_t, KeyGenDecommitment> all_decommitments = {
        {1, decommit1},
        {2, decommit2},
        {3, decommit3}
    };
    
    // ===== Phase 3: ZK Proof =====
    std::cout << "\n--- Phase 3: Generate ZK Proofs ---" << std::endl;
    
    KeyGenZKProof proof1 = node1->Phase3_GenerateZKProof(key_id, all_decommitments);
    KeyGenZKProof proof2 = node2->Phase3_GenerateZKProof(key_id, all_decommitments);
    KeyGenZKProof proof3 = node3->Phase3_GenerateZKProof(key_id, all_decommitments);
    
    std::cout << "✅ All ZK proofs generated" << std::endl;
    
    std::map<uint64_t, KeyGenZKProof> all_zk_proofs = {
        {1, proof1},
        {2, proof2},
        {3, proof3}
    };
    
    // ===== Phase 4: Paillier Proof =====
    std::cout << "\n--- Phase 4: Verify and Generate Paillier Proofs ---" << std::endl;
    
    KeyGenPaillierProof paillier1 = node1->Phase4_VerifyAndGeneratePaillierProof(key_id, all_zk_proofs);
    KeyGenPaillierProof paillier2 = node2->Phase4_VerifyAndGeneratePaillierProof(key_id, all_zk_proofs);
    KeyGenPaillierProof paillier3 = node3->Phase4_VerifyAndGeneratePaillierProof(key_id, all_zk_proofs);
    
    std::cout << "✅ All Paillier proofs generated" << std::endl;
    
    std::map<uint64_t, KeyGenPaillierProof> all_paillier_proofs = {
        {1, paillier1},
        {2, paillier2},
        {3, paillier3}
    };
    
    // ===== Phase 5: 공개키 생성 =====
    std::cout << "\n--- Phase 5: Create Public Key ---" << std::endl;
    
    KeyGenResult result1 = node1->Phase5_CreatePublicKey(key_id, all_paillier_proofs);
    KeyGenResult result2 = node2->Phase5_CreatePublicKey(key_id, all_paillier_proofs);
    KeyGenResult result3 = node3->Phase5_CreatePublicKey(key_id, all_paillier_proofs);
    
    std::cout << "✅ All public keys created" << std::endl;
    
    // ===== 검증 =====
    std::cout << "\n--- Verification ---" << std::endl;
    
    EXPECT_EQ(result1.public_key, result2.public_key);
    EXPECT_EQ(result2.public_key, result3.public_key);
    std::cout << "✅ All nodes generated identical public keys" << std::endl;
    
    EXPECT_EQ(result1.algorithm, CryptoAlgorithm::ECDSA_SECP256K1);
    std::cout << "✅ Algorithm: ECDSA_SECP256K1" << std::endl;
    
    std::cout << "\n📌 Generated Public Key:" << std::endl;
    std::cout << "   Size: " << result1.public_key.size() << " bytes" << std::endl;
    
    std::string hex = BytesToHex(result1.public_key);
    std::cout << "   Hex: " << hex.substr(0, std::min<size_t>(64, hex.size())) << "..." << std::endl;
    
    std::cout << "\n🎉 3/3 Threshold Key Generation SUCCESS!" << std::endl;
}

// ========== Test Case 3: 에러 처리 ==========

TEST_F(FireblocksKeyGenTest, ErrorHandling_InvalidAlgorithm) {
    EXPECT_THROW({
        node1->Phase1_GenerateCommitment(
            key_id, tenant_id, CryptoAlgorithm::UNKNOWN, player_ids, threshold
        );
    }, mpc::MPCException);
}

TEST_F(FireblocksKeyGenTest, ErrorHandling_Phase2_WithoutPhase1) {
    std::map<uint64_t, KeyGenCommitment> empty_commitments;
    
    EXPECT_THROW({
        node1->Phase2_GenerateDecommitment(key_id, empty_commitments);
    }, mpc::MPCException);
}

// ========== Main ==========

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}