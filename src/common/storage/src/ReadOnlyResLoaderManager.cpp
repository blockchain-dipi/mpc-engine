// src/common/resource/src/ReadOnlyResLoaderManager.cpp
#include "common/storage/include/ReadOnlyResLoaderManager.hpp"
#include "common/storage/include/LocalReadOnlyResLoader.hpp"
#include "common/storage/include/AWSReadOnlyResLoader.hpp"
#include "common/storage/include/AzureReadOnlyResLoader.hpp"
#include "common/storage/include/IBMReadOnlyResLoader.hpp"
#include "common/storage/include/GoogleReadOnlyResLoader.hpp"

#include <stdexcept>
#include <sstream>

namespace mpc_engine::resource
{
    ReadOnlyResLoaderManager& ReadOnlyResLoaderManager::Instance()
    {
        static ReadOnlyResLoaderManager instance;
        return instance;
    }

    void ReadOnlyResLoaderManager::Initialize(PlatformType platform_type)
    {
        if (initialized_)
        {
            throw std::runtime_error("ReadOnlyResLoaderManager already initialized");
        }

        platform_type_ = platform_type;

        // 플랫폼에 따라 적절한 구현체 생성
        switch (platform_type_)
        {
            case PlatformType::LOCAL:
                loader_ = std::make_unique<LocalReadOnlyResLoader>();
                break;

            case PlatformType::AWS:
                loader_ = std::make_unique<AWSReadOnlyResLoader>();
                break;

            case PlatformType::AZURE:
                loader_ = std::make_unique<AzureReadOnlyResLoader>();
                break;

            case PlatformType::IBM:
                loader_ = std::make_unique<IBMReadOnlyResLoader>();
                break;

            case PlatformType::GOOGLE:
                loader_ = std::make_unique<GoogleReadOnlyResLoader>();
                break;

            default:
            {
                std::ostringstream oss;
                oss << "Unsupported platform type: " 
                    << static_cast<int>(platform_type_);
                throw std::runtime_error(oss.str());
            }
        }

        // 로더 초기화 확인
        if (!loader_ || !loader_->IsInitialized())
        {
            std::ostringstream oss;
            oss << "Failed to initialize resource loader for platform: "
                << PlatformTypeToString(platform_type_);
            throw std::runtime_error(oss.str());
        }

        initialized_ = true;
    }

    std::string ReadOnlyResLoaderManager::ReadFile(const std::string& path)
    {
        if (!initialized_ || !loader_)
        {
            throw std::runtime_error("ReadOnlyResLoaderManager not initialized");
        }

        return loader_->ReadFile(path);
    }

    std::vector<uint8_t> ReadOnlyResLoaderManager::ReadBinaryFile(const std::string& path)
    {
        if (!initialized_ || !loader_)
        {
            throw std::runtime_error("ReadOnlyResLoaderManager not initialized");
        }

        return loader_->ReadBinaryFile(path);
    }

    bool ReadOnlyResLoaderManager::Exists(const std::string& path)
    {
        if (!initialized_ || !loader_)
        {
            throw std::runtime_error("ReadOnlyResLoaderManager not initialized");
        }

        return loader_->Exists(path);
    }

    bool ReadOnlyResLoaderManager::IsInitialized() const
    {
        return initialized_ && loader_ && loader_->IsInitialized();
    }

    PlatformType ReadOnlyResLoaderManager::GetPlatformType() const
    {
        if (!initialized_)
        {
            throw std::runtime_error("ReadOnlyResLoaderManager not initialized");
        }

        return platform_type_;
    }

} // namespace mpc_engine::resource