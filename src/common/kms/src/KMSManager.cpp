// src/common/kms/src/KMSManager.cpp
#include "common/kms/include/KMSManager.hpp"

namespace mpc_engine::kms {
    // 정적 멤버 변수 정의
    std::shared_ptr<IKeyManagementService> KMSManager::instance = nullptr;
    std::once_flag KMSManager::initialized;
    PlatformType KMSManager::current_platform = PlatformType::UNKNOWN;
}