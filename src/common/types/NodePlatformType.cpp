// // src/common/types/NodePlatformType.cpp
// #include "common/types/NodePlatformType.hpp"

// namespace mpc_engine::node
// {
//     std::string ToString(NodePlatformType type) {
//         switch (type) {
//             case NodePlatformType::LOCAL: return "LOCAL";
//             case NodePlatformType::AWS: return "AWS";
//             case NodePlatformType::IBM: return "IBM";
//             case NodePlatformType::AZURE: return "AZURE";
//             default: return "UNKNOWN";
//         }
//     }

//     NodePlatformType FromString(const std::string& str) {
//         if (str == "LOCAL" || str == "local") return NodePlatformType::LOCAL;
//         if (str == "AWS" || str == "aws") return NodePlatformType::AWS;
//         if (str == "IBM" || str == "ibm") return NodePlatformType::IBM;
//         if (str == "AZURE" || str == "azure") return NodePlatformType::AZURE;
//         return NodePlatformType::UNKNOWN;
//     }
// }