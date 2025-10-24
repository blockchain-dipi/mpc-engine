// mpc-sdk/providers/fireblocks/FireblocksPlatformService.h
#pragma once

#include <cosigner/platform_service.h>
#include <string>
#include <cstdint>
#include <vector>
#include <set>

namespace mpc::providers::fireblocks {

/**
 * @brief Fireblocks platform_service 인터페이스 구현
 * 
 * 역할: 리눅스 OS 의존성 추상화 (난수, 시간 등)
 * 주의: "platform_service"는 AWS/Google 클라우드 서비스가 아님!
 *       단순히 OS 의존성을 추상화하는 인터페이스
 */
class FireblocksPlatformService : public ::fireblocks::common::cosigner::platform_service {
public:
    explicit FireblocksPlatformService(uint64_t player_id, 
                                      const std::string& tenant_id = "default-tenant");
    ~FireblocksPlatformService() override = default;

    // ========== 실제로 구현이 필요한 함수들 ==========
    void gen_random(size_t len, uint8_t* random_data) const override;
    uint64_t now_msec() const override;
    const std::string get_current_tenantid() const override;
    uint64_t get_id_from_keyid(const std::string& key_id) const override;

    // ========== 테스트 코드처럼 더미 구현 ==========
    void derive_initial_share(
        const ::fireblocks::common::cosigner::share_derivation_args& derive_from,
        cosigner_sign_algorithm algorithm,
        elliptic_curve256_scalar_t* key
    ) const override;

    ::fireblocks::common::cosigner::byte_vector_t encrypt_for_player(
        uint64_t id,
        const ::fireblocks::common::cosigner::byte_vector_t& data
    ) const override;

    ::fireblocks::common::cosigner::byte_vector_t decrypt_message(
        const ::fireblocks::common::cosigner::byte_vector_t& encrypted_data
    ) const override;

    bool backup_key(
        const std::string& key_id,
        cosigner_sign_algorithm algorithm,
        const elliptic_curve256_scalar_t& private_key,
        const ::fireblocks::common::cosigner::cmp_key_metadata& metadata,
        const ::fireblocks::common::cosigner::auxiliary_keys& aux
    ) override;

    void on_start_signing(
        const std::string& key_id,
        const std::string& txid,
        const ::fireblocks::common::cosigner::signing_data& data,
        const std::string& metadata_json,
        const std::set<std::string>& players,
        const signing_type signature_type
    ) override;

    void fill_signing_info_from_metadata(
        const std::string& metadata,
        std::vector<uint32_t>& flags
    ) const override;

    bool is_client_id(uint64_t player_id) const override;

private:
    uint64_t player_id_;
    std::string tenant_id_;
};

} // namespace mpc::providers::fireblocks