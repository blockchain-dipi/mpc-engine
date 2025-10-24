// mpc-sdk/providers/fireblocks/FireblocksPersistency.h
#pragma once

#include <cosigner/cmp_setup_service.h>
#include <crypto/elliptic_curve_algebra/elliptic_curve256_algebra.h>
#include <string>
#include <map>
#include <optional>

namespace mpc::providers::fireblocks {

/**
 * @brief Fireblocks setup_key_persistency 인터페이스 구현
 * 
 * 역할: Phase 1-5 동안 중간 데이터를 메모리에 임시 저장
 * 주의: DB 저장 안 함! Node 서버가 데이터를 가져가서 DB에 저장
 */
class FireblocksPersistency : public ::fireblocks::common::cosigner::cmp_setup_service::setup_key_persistency {
public:
    FireblocksPersistency() = default;
    ~FireblocksPersistency() override = default;

    // ========== setup_key_persistency 인터페이스 구현 ==========
    
    bool key_exist(const std::string& key_id) const override;
    
    void load_key(
        const std::string& key_id,
        cosigner_sign_algorithm& algorithm,
        elliptic_curve256_scalar_t& private_key
    ) const override;
    
    const std::string get_tenantid_from_keyid(const std::string& key_id) const override;
    
    void load_key_metadata(
        const std::string& key_id,
        ::fireblocks::common::cosigner::cmp_key_metadata& metadata,
        bool full_load
    ) const override;
    
    void load_auxiliary_keys(
        const std::string& key_id,
        ::fireblocks::common::cosigner::auxiliary_keys& aux
    ) const override;
    
    void store_key(
        const std::string& key_id,
        cosigner_sign_algorithm algorithm,
        const elliptic_curve256_scalar_t& private_key,
        uint64_t ttl = 0
    ) override;
    
    void store_key_metadata(
        const std::string& key_id,
        const ::fireblocks::common::cosigner::cmp_key_metadata& metadata,
        bool allow_override
    ) override;
    
    void store_auxiliary_keys(
        const std::string& key_id,
        const ::fireblocks::common::cosigner::auxiliary_keys& aux
    ) override;
    
    void store_keyid_tenant_id(
        const std::string& key_id,
        const std::string& tenant_id
    ) override;
    
    void store_setup_data(
        const std::string& key_id,
        const ::fireblocks::common::cosigner::setup_data& metadata
    ) override;
    
    void load_setup_data(
        const std::string& key_id,
        ::fireblocks::common::cosigner::setup_data& metadata
    ) override;
    
    void store_setup_commitments(
        const std::string& key_id,
        const std::map<uint64_t, ::fireblocks::common::cosigner::commitment>& commitments
    ) override;
    
    void load_setup_commitments(
        const std::string& key_id,
        std::map<uint64_t, ::fireblocks::common::cosigner::commitment>& commitments
    ) override;
    
    void delete_temporary_key_data(
        const std::string& key_id,
        bool delete_key = false
    ) override;

    // ========== Node가 데이터를 가져가는 Public API ==========
    
    struct KeyInfo {
        cosigner_sign_algorithm algorithm;
        elliptic_curve256_scalar_t private_key;
        std::optional<::fireblocks::common::cosigner::cmp_key_metadata> metadata;
        ::fireblocks::common::cosigner::auxiliary_keys aux_keys;
    };
    
    /**
     * @brief 생성된 키 정보 조회 (Node가 DB 저장용으로 사용)
     */
    const KeyInfo* get_key(const std::string& key_id) const;

    /**
     * @brief 디버그용: 키 덤프 (test_common.h의 dump_key 참고)
     */
    std::string dump_key(const std::string& key_id) const;

private:
    // 메모리 기반 저장소
    std::map<std::string, KeyInfo> keys_;
    std::map<std::string, ::fireblocks::common::cosigner::setup_data> setup_data_;
    std::map<std::string, std::map<uint64_t, ::fireblocks::common::cosigner::commitment>> commitments_;
    std::map<std::string, std::string> keyid_to_tenantid_;
};

} // namespace mpc::providers::fireblocks