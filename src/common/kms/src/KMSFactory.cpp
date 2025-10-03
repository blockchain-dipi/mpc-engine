// src/common/kms/src/KMSFactory.cpp
#include "common/kms/include/KMSFactory.hpp"
#include "common/kms/include/LocalKMS.hpp"
#include "common/kms/include/AwsKMS.hpp"
#include "common/kms/include/AzureKMS.hpp"
#include "common/kms/include/IbmKMS.hpp"
#include "common/kms/include/GoogleKMS.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace mpc_engine::kms
{
    std::unique_ptr<IKeyManagementService> KMSFactory::Create(
        const std::string& provider,
        const std::string& config_path
    )
    {
        std::string normalized = NormalizeProvider(provider);
        
        std::cout << "[KMSFactory] Creating KMS instance for provider: " 
                  << normalized << std::endl;
        
        if (normalized == "local") 
        {
            std::string path = config_path.empty() ? ".kms" : config_path;
            std::cout << "[KMSFactory] Creating LocalKMS with path: " << path << std::endl;
            return std::make_unique<LocalKMS>(path);
        }
        else if (normalized == "aws") 
        {
            std::cout << "[KMSFactory] Creating AwsKMS (stub)" << std::endl;
            return std::make_unique<AwsKMS>();
        }
        else if (normalized == "azure") 
        {
            std::cout << "[KMSFactory] Creating AzureKMS (stub)" << std::endl;
            return std::make_unique<AzureKMS>();
        }
        else if (normalized == "ibm") 
        {
            std::cout << "[KMSFactory] Creating IbmKMS (stub)" << std::endl;
            return std::make_unique<IbmKMS>();
        }
        else if (normalized == "google") 
        {
            std::cout << "[KMSFactory] Creating GoogleKMS (stub)" << std::endl;
            return std::make_unique<GoogleKMS>();
        }
        else 
        {
            std::cerr << "[KMSFactory] Invalid KMS provider: " << provider << std::endl;
            std::cerr << "[KMSFactory] Supported providers: ";
            
            auto providers = GetSupportedProviders();
            for (size_t i = 0; i < providers.size(); ++i) {
                std::cerr << providers[i];
                if (i < providers.size() - 1) {
                    std::cerr << ", ";
                }
            }
            std::cerr << std::endl;
            
            throw std::invalid_argument("Invalid KMS provider: " + provider);
        }
    }

    std::vector<std::string> KMSFactory::GetSupportedProviders() 
    {
        return {
            "local",
            "aws",
            "azure",
            "ibm",
            "google"
        };
    }

    bool KMSFactory::IsValidProvider(const std::string& provider) 
    {
        std::string normalized = NormalizeProvider(provider);
        auto providers = GetSupportedProviders();
        
        return std::find(providers.begin(), providers.end(), normalized) 
               != providers.end();
    }

    std::string KMSFactory::NormalizeProvider(const std::string& provider) 
    {
        std::string normalized = provider;
        
        // 소문자로 변환
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        // 앞뒤 공백 제거
        size_t start = normalized.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return "";
        }
        
        size_t end = normalized.find_last_not_of(" \t\r\n");
        normalized = normalized.substr(start, end - start + 1);
        
        return normalized;
    }
}