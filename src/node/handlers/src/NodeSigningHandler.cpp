// src/node/handlers/src/NodeSigningProtocol.cpp
#include "node/handlers/include/NodeSigningHandler.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "types/MessageTypes.hpp"
#include "common/utils/logger/Logger.hpp"

namespace mpc_engine::node::handlers
{
    std::unique_ptr<CoordinatorNodeMessage> NodeHandleSigningRequest(const CoordinatorNodeMessage* request) 
    {
        LOG_DEBUG("NodeSigningHandler", "=== NodeHandleSigningRequest ===");

        if (!request->has_signing_request()) {
            LOG_ERROR("NodeSigningHandler", "Request does not contain signing_request");
            return nullptr;
        }

        const SigningRequest& signingReq = request->signing_request();

        auto response = std::make_unique<CoordinatorNodeMessage>();
        response->set_message_type(static_cast<int32_t>(mpc_engine::MessageType::SIGNING_REQUEST));
        
        SigningResponse* signingRes = response->mutable_signing_response();

        try 
        {
            LOG_DEBUGF("NodeSigningHandler", "Processing key: %s", signingReq.key_id().c_str());
            LOG_DEBUGF("NodeSigningHandler", "Transaction data (first 50 chars): %s...", signingReq.transaction_data().substr(0, 50).c_str());
            LOG_DEBUGF("NodeSigningHandler", "Threshold: %d", signingReq.threshold());
            LOG_DEBUGF("NodeSigningHandler", "Total shards: %d", signingReq.total_shards());

            ResponseHeader* header = signingRes->mutable_header();
            header->set_success(true);
            header->set_request_id(signingReq.header().request_id());

            // 간단한 시뮬레이션 서명 생성
            signingRes->set_key_id(signingReq.key_id());
            signingRes->set_signature("NODE_MOCK_SIGNATURE_" + signingReq.key_id() + "_" + std::to_string(utils::GetCurrentTimeMs()));
            signingRes->set_shard_index(0);

            LOG_DEBUG("NodeSigningHandler", "Mock signing completed successfully");

        }
        catch (const std::exception& e) 
        {
            LOG_ERRORF("NodeSigningHandler", "Signing failed: %s", e.what());
            ResponseHeader* header = signingRes->mutable_header();
            header->set_success(false);
            header->set_error_message("Signing failed: " + std::string(e.what()));
            header->set_request_id(signingReq.header().request_id());
        }
        
        return response;
    }
}