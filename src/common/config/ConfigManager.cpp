// src/common/config/ConfigManager.cpp
#include "ConfigManager.hpp"
#include <iostream>
#include <stdexcept>

namespace mpc_engine::config
{
    // 정적 멤버 초기화
    std::unique_ptr<ConfigManager> ConfigManager::instance = nullptr;
    std::mutex ConfigManager::instance_mutex;

    ConfigManager& ConfigManager::Instance() 
    {
        std::lock_guard<std::mutex> lock(instance_mutex);
        
        if (!instance) {
            // make_unique는 private 생성자로 인해 사용 불가
            // new를 직접 사용
            instance = std::unique_ptr<ConfigManager>(new ConfigManager());
        }
        
        return *instance;
    }

    bool ConfigManager::Initialize(const std::string& env_type) 
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        
        if (is_initialized) {
            std::cout << "ConfigManager already initialized. Current env: " 
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
        std::cout << "✓ ConfigManager initialized with environment: " << env_type << std::endl;
        return true;
    }

    bool ConfigManager::IsInitialized() const 
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        return is_initialized;
    }

    const EnvConfig& ConfigManager::GetConfig() const 
    {
        EnsureInitialized();
        return *env_config;
    }

    void ConfigManager::EnsureInitialized() const 
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        if (!is_initialized || !env_config) {
            throw std::runtime_error(
                "ConfigManager not initialized. Call ConfigManager::Instance().Initialize(env_type) first."
            );
        }
    }

    std::string ConfigManager::GetString(const std::string& key) const 
    {
        return GetConfig().GetString(key);
    }

    uint16_t ConfigManager::GetUInt16(const std::string& key) const 
    {
        return GetConfig().GetUInt16(key);
    }

    uint32_t ConfigManager::GetUInt32(const std::string& key) const 
    {
        return GetConfig().GetUInt32(key);
    }

    bool ConfigManager::GetBool(const std::string& key) const 
    {
        return GetConfig().GetBool(key);
    }

    std::vector<std::string> ConfigManager::GetStringArray(const std::string& key) const 
    {
        return GetConfig().GetStringArray(key);
    }

    std::vector<uint16_t> ConfigManager::GetUInt16Array(const std::string& key) const 
    {
        return GetConfig().GetUInt16Array(key);
    }

    std::vector<std::pair<std::string, uint16_t>> ConfigManager::GetNodeEndpoints(const std::string& key) const 
    {
        return GetConfig().GetNodeEndpoints(key);
    }

    bool ConfigManager::HasKey(const std::string& key) const 
    {
        return GetConfig().HasKey(key);
    }

    std::string ConfigManager::GetEnvType() const 
    {
        return GetConfig().GetEnvType();
    }

    void ConfigManager::ValidateRequired(const std::vector<std::string>& required_keys) const 
    {
        GetConfig().ValidateRequired(required_keys);
    }

    bool ConfigManager::Reload() 
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        
        if (!is_initialized || !env_config) {
            std::cerr << "Cannot reload: ConfigManager not initialized" << std::endl;
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

    void ConfigManager::PrintLoadedConfig() const 
    {
        if (!IsInitialized()) {
            std::cout << "ConfigManager not initialized" << std::endl;
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

} // namespace mpc_engine::config