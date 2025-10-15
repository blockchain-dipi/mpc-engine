// src/coordinator/handlers/node/src/SigningHandler.cpp
#include "coordinator/handlers/node/include/SigningHandler.hpp"
#include "types/MessageTypes.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include "common/utils/logger/Logger.hpp"
#include <memory>

namespace mpc_engine::coordinator::handlers::node
{
    std::unique_ptr<CoordinatorNodeMessage> HandleSigningRequest(const CoordinatorNodeMessage* request) 
    {
        LOG_DEBUG("SigningHandler", "Signing request received");

        if (!request->has_signing_request()) 
        {
            LOG_ERRORF("SigningHandler", "Request does not contain signing_request");
            return nullptr;
        }

        const SigningRequest& signingReq = request->signing_request();
        auto response = std::make_unique<CoordinatorNodeMessage>();
        response->set_message_type(static_cast<int32_t>(mpc_engine::MessageType::SIGNING_REQUEST));

        SigningResponse* signingRes = response->mutable_signing_response();
        try 
        {
            LOG_DEBUGF("SigningHandler", "Processing key: %s", signingReq.key_id().c_str());
            LOG_DEBUGF("SigningHandler", "Transaction: %s...", signingReq.transaction_data().substr(0, 50).c_str());
            LOG_DEBUGF("SigningHandler", "Threshold: %d", signingReq.threshold());
            LOG_DEBUGF("SigningHandler", "Total shards: %d", signingReq.total_shards());

            ResponseHeader* header = signingRes->mutable_header();
            header->set_success(true);
            header->set_request_id(signingReq.header().request_id());
            
            signingRes->set_key_id(signingReq.key_id());
            signingRes->set_signature("MOCK_SIGNATURE_" + signingReq.key_id() + "_" + std::to_string(utils::GetCurrentTimeMs()));
            signingRes->set_shard_index(0);

            LOG_DEBUG("SigningHandler", "Mock signing completed successfully");
        }
        catch (const std::exception& e) 
        {
            ResponseHeader* header = signingRes->mutable_header();
            header->set_success(false);
            header->set_error_message("Signing failed: " + std::string(e.what()));
            header->set_request_id(signingReq.header().request_id());
        }
        
        return response;
    }
} // namespace mpc_engine::coordinator::handlers::node