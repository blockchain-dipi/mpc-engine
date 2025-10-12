// src/coordinator/handlers/node/include/MessageRouter.hpp
#pragma once
#include "BaseProtocol.hpp"
#include <memory>

namespace mpc_engine::protocol::coordinator_node
{
    struct SigningRequest : public BaseRequest 
    {
        std::string keyId;
        std::string transactionData;
        uint32_t threshold = 2;
        uint32_t totalShards = 3;

        SigningRequest() : BaseRequest(MessageType::SIGNING_REQUEST) {}
    };

    struct SigningResponse : public BaseResponse
     {
        std::string keyId;
        std::string signature;
        uint32_t shardIndex = 0;

        SigningResponse() : BaseResponse(MessageType::SIGNING_REQUEST) {}
    };
}