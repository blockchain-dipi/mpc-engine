// src/coordinator/handlers/node/include/SigningHandler.hpp
#pragma once
#include "proto/coordinator_node/generated/message.pb.h"
#include <memory>

namespace mpc_engine::coordinator::handlers::node
{
    using namespace mpc_engine::proto::coordinator_node;
    std::unique_ptr<CoordinatorNodeMessage> HandleSigningRequest(const CoordinatorNodeMessage* request);
}