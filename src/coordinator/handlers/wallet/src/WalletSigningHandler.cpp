// src/coordinator/handlers/wallet/src/WalletSigningHandler.cpp
#include "coordinator/handlers/wallet/include/WalletSigningHandler.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/utils/logger/Logger.hpp"

namespace mpc_engine::coordinator::handlers::wallet
{
    std::unique_ptr<WalletCoordinatorMessage> HandleWalletSigningRequest(const WalletCoordinatorMessage* request) 
    {
        LOG_DEBUG("WalletSigningHandler", "=== HandleWalletSigningRequest ===");

        if (!request || !request->has_signing_request()) {
            LOG_ERROR("WalletSigningHandler", "[Handler] Invalid request");
            return nullptr;
        }

        const WalletSigningRequest& signing_req = request->signing_request();
        
        auto response_msg = std::make_unique<WalletCoordinatorMessage>();
        response_msg->set_message_type(signing_req.header().message_type());
        
        auto* response = response_msg->mutable_signing_response();
        auto* header = response->mutable_header();

        try 
        {
            LOG_DEBUG("WalletSigningHandler", "[Handler] Processing signing request:");
            LOG_DEBUGF("WalletSigningHandler", "  Key ID: %s", signing_req.key_id().c_str());
            LOG_DEBUGF("WalletSigningHandler", "  Transaction: %s", signing_req.transaction_data().substr(0, 50).c_str());
            LOG_DEBUGF("WalletSigningHandler", "  Threshold: %d/%d", signing_req.threshold(), signing_req.total_shards());

            // === Mock MPC 서명 시뮬레이션 ===
            
            // 1. 헤더 설정
            header->set_message_type(signing_req.header().message_type());
            header->set_success(true);
            header->set_request_id(signing_req.header().request_id());
            header->set_timestamp(std::to_string(utils::GetCurrentTimeMs()));

            // 2. Shard 서명 생성 (Mock)
            response->set_key_id(signing_req.key_id());
            
            for (uint32_t i = 0; i < signing_req.total_shards(); ++i) {
                std::string shard_sig 
                    = "0xMOCK_SHARD_" + std::to_string(i) + "_" + signing_req.key_id() + "_" + std::to_string(utils::GetCurrentTimeMs());
                response->add_shard_signatures(shard_sig);
            }

            // 3. 최종 서명 생성 (Mock)
            std::string final_sig = "0xMOCK_FINAL_SIG_" + signing_req.key_id() + "_" + std::to_string(utils::GetCurrentTimeMs());
            response->set_final_signature(final_sig);
            response->set_successful_shards(signing_req.total_shards());

            LOG_DEBUG("WalletSigningHandler", "[Handler] Mock signing completed successfully");
            LOG_DEBUGF("WalletSigningHandler", "  Final Signature: %s", response->final_signature().c_str());
        }
        catch (const std::exception& e) 
        {
            header->set_success(false);
            header->set_error_message("Wallet signing failed: " + std::string(e.what()));
            LOG_ERRORF("WalletSigningHandler", "Error: %s", header->error_message().c_str());
        }
        
        return response_msg;
    }

} // namespace mpc_engine::coordinator::handlers::wallet