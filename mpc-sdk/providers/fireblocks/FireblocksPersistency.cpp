// mpc-sdk/providers/fireblocks/FireblocksPersistency.cpp
#include "FireblocksPersistency.h"
#include <cosigner/cosigner_exception.h>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace mpc::providers::fireblocks {

using namespace ::fireblocks::common::cosigner;

// ========== Helper: 디버그용 HexStr 함수 ==========
template<typename T>
static std::string HexStr(const T* begin, const T* end) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const T* it = begin; it < end; ++it) {
        oss << std::setw(2) << static_cast<unsigned int>(static_cast<uint8_t>(*it));
    }
    return oss.str();
}

// ========== 키 존재 여부 확인 ==========
bool FireblocksPersistency::key_exist(const std::string& key_id) const {
    return keys_.find(key_id) != keys_.end();
}

// ========== 키 로드 ==========
void FireblocksPersistency::load_key(
    const std::string& key_id,
    cosigner_sign_algorithm& algorithm,
    elliptic_curve256_scalar_t& private_key
) const {
    auto it = keys_.find(key_id);
    if (it == keys_.end()) {
        throw cosigner_exception(cosigner_exception::BAD_KEY);
    }
    
    std::memcpy(private_key, it->second.private_key, sizeof(elliptic_curve256_scalar_t));
    algorithm = it->second.algorithm;
}

// ========== Tenant ID 조회 ==========
const std::string FireblocksPersistency::get_tenantid_from_keyid(const std::string& key_id) const {
    auto it = keyid_to_tenantid_.find(key_id);
    if (it != keyid_to_tenantid_.end()) {
        return it->second;
    }
    return "default-tenant";  // 기본값
}

// ========== 키 메타데이터 로드 ==========
void FireblocksPersistency::load_key_metadata(
    const std::string& key_id,
    cmp_key_metadata& metadata,
    bool /*full_load*/
) const {
    auto it = keys_.find(key_id);
    if (it == keys_.end()) {
        throw cosigner_exception(cosigner_exception::BAD_KEY);
    }
    
    if (!it->second.metadata) {
        throw cosigner_exception(cosigner_exception::BAD_KEY);
    }
    
    metadata = it->second.metadata.value();
}

// ========== Auxiliary Keys 로드 ==========
void FireblocksPersistency::load_auxiliary_keys(
    const std::string& key_id,
    auxiliary_keys& aux
) const {
    auto it = keys_.find(key_id);
    if (it == keys_.end()) {
        throw cosigner_exception(cosigner_exception::BAD_KEY);
    }
    
    aux = it->second.aux_keys;
}

// ========== 키 저장 ==========
void FireblocksPersistency::store_key(
    const std::string& key_id,
    cosigner_sign_algorithm algorithm,
    const elliptic_curve256_scalar_t& private_key,
    uint64_t /*ttl*/
) {
    auto& info = keys_[key_id];
    std::memcpy(info.private_key, private_key, sizeof(elliptic_curve256_scalar_t));
    info.algorithm = algorithm;
}

// ========== 키 메타데이터 저장 ==========
void FireblocksPersistency::store_key_metadata(
    const std::string& key_id,
    const cmp_key_metadata& metadata,
    bool allow_override
) {
    auto& info = keys_[key_id];
    
    if (!allow_override && info.metadata) {
        throw cosigner_exception(cosigner_exception::INTERNAL_ERROR);
    }
    
    info.metadata = metadata;
}

// ========== Auxiliary Keys 저장 ==========
void FireblocksPersistency::store_auxiliary_keys(
    const std::string& key_id,
    const auxiliary_keys& aux
) {
    auto& info = keys_[key_id];
    info.aux_keys = aux;
}

// ========== Tenant ID 매핑 저장 ==========
void FireblocksPersistency::store_keyid_tenant_id(
    const std::string& key_id,
    const std::string& tenant_id
) {
    keyid_to_tenantid_[key_id] = tenant_id;
}

// ========== Setup Data 저장 ==========
void FireblocksPersistency::store_setup_data(
    const std::string& key_id,
    const setup_data& metadata
) {
    setup_data_[key_id] = metadata;
}

// ========== Setup Data 로드 ==========
void FireblocksPersistency::load_setup_data(
    const std::string& key_id,
    setup_data& metadata
) {
    auto it = setup_data_.find(key_id);
    if (it == setup_data_.end()) {
        throw cosigner_exception(cosigner_exception::BAD_KEY);
    }
    metadata = it->second;
}

// ========== Setup Commitments 저장 ==========
void FireblocksPersistency::store_setup_commitments(
    const std::string& key_id,
    const std::map<uint64_t, commitment>& commitments
) {
    if (commitments_.find(key_id) != commitments_.end()) {
        throw cosigner_exception(cosigner_exception::INTERNAL_ERROR);
    }
    
    commitments_[key_id] = commitments;
}

// ========== Setup Commitments 로드 ==========
void FireblocksPersistency::load_setup_commitments(
    const std::string& key_id,
    std::map<uint64_t, commitment>& commitments
) {
    auto it = commitments_.find(key_id);
    if (it == commitments_.end()) {
        throw cosigner_exception(cosigner_exception::BAD_KEY);
    }
    commitments = it->second;
}

// ========== 임시 데이터 삭제 ==========
void FireblocksPersistency::delete_temporary_key_data(
    const std::string& key_id,
    bool delete_key
) {
    setup_data_.erase(key_id);
    commitments_.erase(key_id);
    
    if (delete_key) {
        keys_.erase(key_id);
    }
}

// ========== Public API: 키 조회 ==========
const FireblocksPersistency::KeyInfo* FireblocksPersistency::get_key(const std::string& key_id) const {
    auto it = keys_.find(key_id);
    if (it == keys_.end()) {
        return nullptr;
    }
    return &it->second;
}

// ========== Public API: 키 덤프 (디버그용) ==========
std::string FireblocksPersistency::dump_key(const std::string& key_id) const {
    auto it = keys_.find(key_id);
    if (it == keys_.end()) {
        throw cosigner_exception(cosigner_exception::BAD_KEY);
    }
    
    return HexStr(
        it->second.private_key,
        it->second.private_key + sizeof(elliptic_curve256_scalar_t)
    );
}

} // namespace mpc::providers::fireblocks