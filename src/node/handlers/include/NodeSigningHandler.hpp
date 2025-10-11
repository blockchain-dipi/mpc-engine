// src/node/handlers/include/NodeSigningHandler.hpp
#pragma once
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <memory>

namespace mpc_engine::node::handlers
{
    using namespace protocol::coordinator_node;
    std::unique_ptr<BaseResponse> NoeHandleSigningRequest(const BaseRequest* request);
}