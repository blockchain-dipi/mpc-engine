// src/coordinator/handlers/node/src/SigningHandler.cpp
#include "coordinator/handlers/node/include/SigningHandler.hpp"
#include "types/MessageTypes.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>
#include <memory>

namespace mpc_engine::coordinator::handlers::node
{
    std::unique_ptr<CoordinatorNodeMessage> HandleSigningRequest(const CoordinatorNodeMessage* request) 
    {
        std::cout << "=== HandleSigningRequest ===" << std::endl;

        if (!request->has_signing_request()) 
        {
            std::cerr << "Request does not contain signing_request" << std::endl;
            return nullptr;
        }

        const SigningRequest& signingReq = request->signing_request();
        auto response = std::make_unique<CoordinatorNodeMessage>();
        response->set_message_type(static_cast<int32_t>(mpc_engine::MessageType::SIGNING_REQUEST));

        SigningResponse* signingRes = response->mutable_signing_response();
        try 
        {
            std::cout << "Processing key: " << signingReq.key_id() << std::endl;
            std::cout << "Transaction: " << signingReq.transaction_data().substr(0, 50) << "..." << std::endl;
            std::cout << "Threshold: " << signingReq.threshold() << std::endl;
            std::cout << "Total shards: " << signingReq.total_shards() << std::endl;

            ResponseHeader* header = signingRes->mutable_header();
            header->set_success(true);
            header->set_request_id(signingReq.header().request_id());
            
            signingRes->set_key_id(signingReq.key_id());
            signingRes->set_signature("MOCK_SIGNATURE_" + signingReq.key_id() + "_" + std::to_string(utils::GetCurrentTimeMs()));
            signingRes->set_shard_index(0);
            
            std::cout << "Mock signing completed successfully" << std::endl;            
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