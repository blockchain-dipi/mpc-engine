// src/common/kms/include/KMSManager.hpp
#pragma once
#include "IKeyManagementService.hpp"
#include "LocalKMS.hpp"
#include "AwsKMS.hpp"
#include "AzureKMS.hpp"
#include "IbmKMS.hpp"
#include "GoogleKMS.hpp"
#include "common/types/BasicTypes.hpp"
#include <cassert>

namespace mpc_engine::kms
{
    class KMSManager 
    {
    private:
        static std::shared_ptr<IKeyManagementService> instance;
        static std::once_flag initialized;
        static PlatformType current_platform;

    public:
        static void Initialize(PlatformType platform) 
        {
            std::call_once(initialized, [platform]() {
                current_platform = platform;
                switch (platform) {
                    case PlatformType::LOCAL:
                        instance = std::make_shared<LocalKMS>("./kms");
                        break;
                        case PlatformType::AWS:
                        instance = std::make_shared<AwsKMS>();
                        break;
                    case PlatformType::AZURE:
                        instance = std::make_shared<AzureKMS>();
                        break;
                    case PlatformType::IBM:
                        instance = std::make_shared<IbmKMS>();
                        break;
                    case PlatformType::GOOGLE:
                        instance = std::make_shared<GoogleKMS>();
                        break;
                    default:
                        throw std::invalid_argument("Unsupported KMS platform");
                        break;
                }
                instance->Initialize();
            });
        }

        static void InitializeLocal(PlatformType platform, const std::string& config_path) 
        {
            assert(platform == PlatformType::LOCAL);

            std::call_once(initialized, [platform, config_path]() {
                current_platform = platform;
                instance = std::make_shared<LocalKMS>(config_path);
              instance->Initialize();
            });
        }

        static IKeyManagementService& Instance() 
        {
            if (!instance) {
                throw std::runtime_error("KMSManager not initialized");
            }
            return *instance;
        }
    };
}
