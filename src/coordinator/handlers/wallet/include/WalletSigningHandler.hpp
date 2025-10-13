// src/coordinator/handlers/wallet/include/WalletSigningHandler.hpp
#pragma once
#include "proto/wallet_coordinator/generated/wallet_message.pb.h"
#include <memory>

namespace mpc_engine::coordinator::handlers::wallet
{
    using namespace mpc_engine::proto::wallet_coordinator;
    std::unique_ptr<WalletCoordinatorMessage> HandleWalletSigningRequest(const WalletCoordinatorMessage* request);

} // namespace mpc_engine::coordinator::handlers::wallet