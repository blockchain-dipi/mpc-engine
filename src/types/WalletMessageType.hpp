// src/types/WalletMessageType.hpp
#pragma once
#include <cstdint>

namespace mpc_engine
{
    /**
     * @brief Wallet 서버와의 통신 프로토콜 타입
     */
    enum class WalletMessageType : uint32_t 
    {
        SIGNING_REQUEST = 1001,    // 서명 요청 프로토콜
        MAX_MESSAGE_TYPE
    };

    inline const char* WalletMessageTypeToString(WalletMessageType type)
    {
        switch (type) {
            case WalletMessageType::SIGNING_REQUEST:
                return "SIGNING_REQUEST";
            default:
                return "UNKNOWN";
        }
    }
} // namespace mpc_engine::protocol::coordinator_wallet