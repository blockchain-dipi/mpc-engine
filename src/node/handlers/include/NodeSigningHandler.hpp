// src/node/handlers/include/NodeSigningHandler.hpp
#pragma once
#include "proto/coordinator_node/generated/message.pb.h"

namespace mpc_engine::node::handlers
{
    using namespace mpc_engine::proto::coordinator_node;
    std::unique_ptr<CoordinatorNodeMessage> NodeHandleSigningRequest(const CoordinatorNodeMessage* request);
}