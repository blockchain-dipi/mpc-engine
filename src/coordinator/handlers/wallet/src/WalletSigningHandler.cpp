// src/coordinator/handlers/wallet/src/WalletSigningHandler.cpp
#include "coordinator/handlers/wallet/include/WalletSigningHandler.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>

namespace mpc_engine::coordinator::handlers::wallet
{
    std::unique_ptr<WalletCoordinatorMessage> HandleWalletSigningRequest(const WalletCoordinatorMessage* request) 
    {
        std::cout << "=== HandleWalletSigningRequest ===" << std::endl;

        if (!request || !request->has_signing_request()) {
            std::cerr << "[Handler] Invalid request" << std::endl;
            return nullptr;
        }

        const WalletSigningRequest& signing_req = request->signing_request();
        
        auto response_msg = std::make_unique<WalletCoordinatorMessage>();
        response_msg->set_message_type(signing_req.header().message_type());
        
        auto* response = response_msg->mutable_signing_response();
        auto* header = response->mutable_header();

        try 
        {
            std::cout << "[Handler] Processing signing request:" << std::endl;
            std::cout << "  Key ID: " << signing_req.key_id() << std::endl;
            std::cout << "  Transaction: " << signing_req.transaction_data().substr(0, 50) << "..." << std::endl;
            std::cout << "  Threshold: " << signing_req.threshold() << "/" << signing_req.total_shards() << std::endl;

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
            
            std::cout << "[Handler] Mock signing completed successfully" << std::endl;
            std::cout << "  Final Signature: " << response->final_signature() << std::endl;
        }
        catch (const std::exception& e) 
        {
            header->set_success(false);
            header->set_error_message("Wallet signing failed: " + std::string(e.what()));
            std::cerr << "[Handler] Error: " << header->error_message() << std::endl;
        }
        
        return response_msg;
    }

} // namespace mpc_engine::coordinator::handlers::wallet