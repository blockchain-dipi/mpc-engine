// mpc-sdk/providers/fireblocks/FireblocksPlatformService.cpp
#include "FireblocksPlatformService.h"
#include <openssl/rand.h>
#include <chrono>
#include <cassert>
#include <stdexcept>

namespace mpc::providers::fireblocks {

using namespace ::fireblocks::common::cosigner;

// 고정밀 시계 선택 (steady_clock 우선)
using Clock = std::conditional<
    std::chrono::high_resolution_clock::is_steady,
    std::chrono::high_resolution_clock,
    std::chrono::steady_clock
>::type;

FireblocksPlatformService::FireblocksPlatformService(uint64_t player_id, 
                                                    const std::string& tenant_id)
    : player_id_(player_id)
    , tenant_id_(tenant_id)
{
}

// ========== 실제 구현 ==========

void FireblocksPlatformService::gen_random(size_t len, uint8_t* random_data) const {
    if (RAND_bytes(random_data, static_cast<int>(len)) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }
}

uint64_t FireblocksPlatformService::now_msec() const {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(
        Clock::now()
    ).time_since_epoch().count();
}

const std::string FireblocksPlatformService::get_current_tenantid() const {
    return tenant_id_;
}

uint64_t FireblocksPlatformService::get_id_from_keyid(const std::string& /*key_id*/) const {
    return player_id_;
}

// ========== 더미 구현 (테스트 코드처럼) ==========

void FireblocksPlatformService::derive_initial_share(
    const share_derivation_args& /*derive_from*/,
    cosigner_sign_algorithm /*algorithm*/,
    elliptic_curve256_scalar_t* /*key*/
) const {
    assert(0 && "derive_initial_share not implemented");
}

byte_vector_t FireblocksPlatformService::encrypt_for_player(
    uint64_t /*id*/,
    const byte_vector_t& data
) const {
    return data;  // 암호화 안 함 (동일 서버 내 통신 가정)
}

byte_vector_t FireblocksPlatformService::decrypt_message(
    const byte_vector_t& encrypted_data
) const {
    return encrypted_data;  // 복호화 안 함
}

bool FireblocksPlatformService::backup_key(
    const std::string& /*key_id*/,
    cosigner_sign_algorithm /*algorithm*/,
    const elliptic_curve256_scalar_t& /*private_key*/,
    const cmp_key_metadata& /*metadata*/,
    const auxiliary_keys& /*aux*/
) {
    return true;  // 백업하지 않음
}

void FireblocksPlatformService::on_start_signing(
    const std::string& /*key_id*/,
    const std::string& /*txid*/,
    const signing_data& /*data*/,
    const std::string& /*metadata_json*/,
    const std::set<std::string>& /*players*/,
    const signing_type /*signature_type*/
) {
    // 빈 구현
}

void FireblocksPlatformService::fill_signing_info_from_metadata(
    const std::string& /*metadata*/,
    std::vector<uint32_t>& /*flags*/
) const {
    // 빈 구현
}

bool FireblocksPlatformService::is_client_id(uint64_t /*player_id*/) const {
    return false;  // 모든 플레이어가 서버
}

} // namespace mpc::providers::fireblocks