// src/node/handlers/src/NodeSigningProtocol.cpp
#include "protocols/coordinator_node/include/SigningProtocol.hpp"
#include "node/handlers/include/NodeSigningHandler.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>
#include <memory>

namespace mpc_engine::node::handlers
{
    using namespace mpc_engine::protocol::coordinator_node;
    
    std::unique_ptr<BaseResponse> NoeHandleSigningRequest(const BaseRequest* request) 
    {
        std::cout << "=== NoeHandleSigningRequest ===" << std::endl;

        const SigningRequest* signingReq = static_cast<const SigningRequest*>(request);
        std::unique_ptr<SigningResponse> response = std::make_unique<SigningResponse>();

        response->keyId = signingReq->keyId;

        try 
        {
            std::cout << "Processing key: " << signingReq->keyId << std::endl;
            std::cout << "Transaction: " << signingReq->transactionData.substr(0, 50) << "..." << std::endl;

            // 간단한 시뮬레이션 서명 생성
            response->signature = "MOCK_SIGNATURE_" + signingReq->keyId + "_" + std::to_string(utils::GetCurrentTimeMs());
            response->shardIndex = 0;
            response->success = true;
            
            std::cout << "Mock signing completed successfully" << std::endl;
            
        }
        catch (const std::exception& e) 
        {
            response->success = false;
            response->errorMessage = "Signing failed: " + std::string(e.what());
        }
        
        return response;
    }
}