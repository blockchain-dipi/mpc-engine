// src/node/handlers/include/SigningHandler.hpp
#pragma once
#include "protocols/coordinator_node/include/BaseProtocol.hpp"
#include <memory>

namespace mpc_engine::coordinator::handlers::node
{
    using namespace mpc_engine::protocol::coordinator_node;
    std::unique_ptr<BaseResponse> HandleSigningRequest(const BaseRequest* request);
}