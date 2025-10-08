// src/common/network/tls/include/TlsCertificateLoader.hpp
#pragma once

#include "TlsContext.hpp"
#include "common/types/NodePlatformType.hpp"
#include <string>

namespace mpc_engine::network::tls
{
    using namespace mpc_engine::node;
    
    class TlsCertificateLoader 
    {
    public:
        explicit TlsCertificateLoader(NodePlatformType platform);

        std::string LoadCaCertificate();
        CertificateData LoadCoordinatorCertificate();
        CertificateData LoadNodeCertificate(int node_index);
        bool IsHealthy();
        void PrintStatus();

    private:
        NodePlatformType platform_;
        
        bool IsLocalPlatform();
        std::string ReadPemFile(const std::string& file_path);
        
        std::string GetCoordinatorCertPath();
        std::string GetCoordinatorKeyPath();
        std::string GetNodeCertPath(int node_index);
        std::string GetNodeKeyPath(int node_index);
    };
}