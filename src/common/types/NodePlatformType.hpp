// src/common/types/NodePlatformType.hpp
#pragma once
#include <string>

namespace mpc_engine::node
{
    enum class NodePlatformType 
    {
        LOCAL = 0,
        AWS = 1,
        IBM = 2,
        AZURE = 3,
        UNKNOWN = 99
    };

    std::string ToString(NodePlatformType type);
    NodePlatformType FromString(const std::string& str);
}