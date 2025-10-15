// src/common/env/EnvManager.cpp
#include "EnvManager.hpp"
#include <iostream>
#include <stdexcept>

namespace mpc_engine::env
{
    // 정적 멤버 초기화
    std::unique_ptr<EnvManager> EnvManager::instance = nullptr;
    std::mutex EnvManager::instance_mutex;

    EnvManager& EnvManager::Instance() 
    {
        std::lock_guard<std::mutex> lock(instance_mutex);
        
        if (!instance) {
            // make_unique는 private 생성자로 인해 사용 불가
            // new를 직접 사용
            instance = std::unique_ptr<EnvManager>(new EnvManager());
        }
        
        return *instance;
    }

    bool EnvManager::Initialize(const std::string& env_type) 
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        
        if (is_initialized) {
            std::cout << "EnvManager already initialized. Current env: " 
                      << env_config->GetEnvType() << ", Requested: " << env_type << std::endl;
            return env_config->GetEnvType() == env_type;
        }

        env_config = std::make_unique<EnvConfig>();
        
        if (!env_config->LoadFromEnv(env_type)) {
            std::cerr << "Failed to load environment configuration: " << env_type << std::endl;
            env_config.reset();
            return false;
        }

        is_initialized = true;
        std::cout << "✓ EnvManager initialized with environment: " << env_type << std::endl;
        return true;
    }

    bool EnvManager::IsInitialized() const 
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        return is_initialized;
    }

    const EnvConfig& EnvManager::GetConfig() const 
    {
        EnsureInitialized();
        return *env_config;
    }

    void EnvManager::EnsureInitialized() const 
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        if (!is_initialized || !env_config) {
            throw std::runtime_error(
                "EnvManager not initialized. Call EnvManager::Instance().Initialize(env_type) first."
            );
        }
    }

    std::string EnvManager::GetString(const std::string& key) const 
    {
        return GetConfig().GetString(key);
    }

    uint16_t EnvManager::GetUInt16(const std::string& key) const 
    {
        return GetConfig().GetUInt16(key);
    }

    uint32_t EnvManager::GetUInt32(const std::string& key) const 
    {
        return GetConfig().GetUInt32(key);
    }

    bool EnvManager::GetBool(const std::string& key) const 
    {
        return GetConfig().GetBool(key);
    }

    std::vector<std::string> EnvManager::GetStringArray(const std::string& key) const 
    {
        return GetConfig().GetStringArray(key);
    }

    std::vector<uint16_t> EnvManager::GetUInt16Array(const std::string& key) const 
    {
        return GetConfig().GetUInt16Array(key);
    }

    std::vector<std::pair<std::string, uint16_t>> EnvManager::GetNodeEndpoints(const std::string& key) const 
    {
        return GetConfig().GetNodeEndpoints(key);
    }

    bool EnvManager::HasKey(const std::string& key) const 
    {
        return GetConfig().HasKey(key);
    }

    std::string EnvManager::GetEnvType() const 
    {
        return GetConfig().GetEnvType();
    }

    void EnvManager::ValidateRequired(const std::vector<std::string>& required_keys) const 
    {
        GetConfig().ValidateRequired(required_keys);
    }

    bool EnvManager::Reload() 
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        
        if (!is_initialized || !env_config) {
            std::cerr << "Cannot reload: EnvManager not initialized" << std::endl;
            return false;
        }

        std::string current_env = env_config->GetEnvType();
        env_config.reset();
        
        env_config = std::make_unique<EnvConfig>();
        if (!env_config->LoadFromEnv(current_env)) {
            std::cerr << "Failed to reload environment configuration: " << current_env << std::endl;
            is_initialized = false;
            env_config.reset();
            return false;
        }

        std::cout << "✓ Configuration reloaded successfully" << std::endl;
        return true;
    }

    void EnvManager::PrintLoadedConfig() const 
    {
        if (!IsInitialized()) {
            std::cout << "EnvManager not initialized" << std::endl;
            return;
        }

        std::cout << "\n=== Current Configuration ===" << std::endl;
        std::cout << "Environment: " << GetEnvType() << std::endl;
        
        // 주요 설정들만 출력 (보안상 민감한 정보는 제외)
        std::vector<std::string> safe_keys = {
            "COORDINATOR_HOST", "COORDINATOR_PORT", "COORDINATOR_PLATFORM",
            "NODE_IDS", "NODE_HOSTS", "NODE_PLATFORMS",
            "MPC_THRESHOLD", "MPC_TOTAL_SHARDS",
            "LOG_LEVEL", "CONNECTION_TIMEOUT_MS"
        };

        for (const auto& key : safe_keys) {
            if (HasKey(key)) {
                std::cout << "  " << key << ": " << GetString(key) << std::endl;
            }
        }
        std::cout << "=============================" << std::endl;
    }

} // namespace mpc_engine::env