// src/common/kms/include/KMSManager.hpp
#pragma once

#include "LocalKMS.hpp"
#include "common/types/NodePlatformType.hpp"
#include <memory>
#include <mutex>

namespace mpc_engine::kms
{
    using namespace mpc_engine::node;
    
    class KMSManager 
    {
    public:
        static std::shared_ptr<IKeyManagementService> GetInstance(NodePlatformType platform)
        {
            static std::once_flag initialized;
            static std::shared_ptr<IKeyManagementService> instance;
            
            std::call_once(initialized, [platform]() {
                switch (platform) {
                    case NodePlatformType::LOCAL:
                        instance = std::make_shared<LocalKMS>("./kms");
                        break;
                    case NodePlatformType::AWS:
                        // instance = std::make_shared<AWSKMS>();
                        throw std::runtime_error("AWS KMS not implemented yet");
                    case NodePlatformType::AZURE:
                        // instance = std::make_shared<AzureKMS>();
                        throw std::runtime_error("Azure KMS not implemented yet");
                    case NodePlatformType::IBM:
                        // instance = std::make_shared<IBMKMS>();
                        throw std::runtime_error("IBM KMS not implemented yet");
                    case NodePlatformType::GOOGLE:
                        // instance = std::make_shared<GoogleKMS>();
                        throw std::runtime_error("Google KMS not implemented yet");
                    default:
                        throw std::runtime_error("Unknown platform type");
                }
                instance->Initialize();
            });
            
            return instance;
        }
    };
}