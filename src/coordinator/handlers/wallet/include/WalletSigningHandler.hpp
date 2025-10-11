// src/coordinator/handlers/wallet/include/WalletSigningHandler.hpp
#pragma once
#include "protocols/coordinator_wallet/include/WalletProtocolTypes.hpp"
#include <memory>

namespace mpc_engine::coordinator::handlers::wallet
{
    /**
     * @brief Wallet 서명 요청 처리
     * 
     * @param request WalletSigningRequest
     * @return WalletSigningResponse (Mock 데이터)
     */
    std::unique_ptr<protocol::coordinator_wallet::WalletBaseResponse> 
    HandleWalletSigningRequest(const protocol::coordinator_wallet::WalletBaseRequest* request);

} // namespace mpc_engine::coordinator::handlers::wallet