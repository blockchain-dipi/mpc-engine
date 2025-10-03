// src/common/kms/src/LocalKMS.cpp
#include "common/kms/include/LocalKMS.hpp"
#include "common/kms/include/KMSException.hpp"
#include <fstream>
#include <iostream>

namespace mpc_engine::kms
{
    LocalKMS::LocalKMS(const std::string& path) 
        : storage_path(path), is_initialized(false) 
    {
        std::cout << "[LocalKMS] Initialized with storage path: " << storage_path << std::endl;
    }

    LocalKMS::~LocalKMS() 
    {
    }

    bool LocalKMS::Initialize() 
    {
        std::lock_guard<std::mutex> lock(storage_mutex);
        
        if (is_initialized) {
            return true;
        }
        
        try {
            if (!fs::exists(storage_path)) {
                if (!fs::create_directories(storage_path)) {
                    std::cerr << "[LocalKMS] Failed to create storage directory: " << storage_path << std::endl;
                    return false;
                }
                std::cout << "[LocalKMS] Created storage directory: \"" << fs::absolute(storage_path).string() << "\"" << std::endl;
            }
            
            is_initialized = true;
            std::cout << "[LocalKMS] Successfully initialized" << std::endl;
            std::cout << "[LocalKMS] Storage path: \"" << fs::absolute(storage_path).string() << "\"" << std::endl;
            return true;
            
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[LocalKMS] Filesystem error during initialization: " << e.what() << std::endl;
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[LocalKMS] Exception during initialization: " << e.what() << std::endl;
            return false;
        }
    }

    bool LocalKMS::IsInitialized() const 
    {
        return is_initialized;
    }

    bool LocalKMS::PutSecret(const std::string& key, const std::string& value)
    {
        std::lock_guard<std::mutex> lock(storage_mutex);
        
        if (!is_initialized) {
            throw KMSException("LocalKMS is not initialized");
        }
        
        fs::path key_file = storage_path / (key + ".key");
        
        try {
            // 🆕 기존 파일이 있으면 쓰기 권한 복구
            if (fs::exists(key_file)) {
                fs::permissions(key_file, 
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
            }
            
            std::ofstream file(key_file, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                std::cerr << "[LocalKMS] Failed to write secret: " << key << std::endl;
                return false;
            }
            
            file.write(value.data(), value.size());
            file.close();
            
            // 쓰기 완료 후 읽기 전용으로 설정
            fs::permissions(key_file, 
                fs::perms::owner_read,
                fs::perm_options::replace);
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[LocalKMS] Exception storing secret '" << key << "': " << e.what() << std::endl;
            return false;
        }
    }

    std::string LocalKMS::GetSecret(const std::string& key) const
    {
        std::lock_guard<std::mutex> lock(storage_mutex);
        
        if (!is_initialized) {
            throw KMSException("LocalKMS is not initialized");
        }
        
        fs::path key_file = storage_path / (key + ".key");
        
        if (!fs::exists(key_file)) {
            throw SecretNotFoundException(key);
        }
        
        try {
            std::ifstream file(key_file, std::ios::binary);
            if (!file.is_open()) {
                throw KMSException("Failed to open secret file: " + key);
            }
            
            std::string value((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
            
            return value;
            
        } catch (const SecretNotFoundException&) {
            throw;
        } catch (const std::exception& e) {
            throw KMSException("Failed to read secret '" + key + "': " + std::string(e.what()));
        }
    }

    bool LocalKMS::SecretExists(const std::string& key) const
    {
        std::lock_guard<std::mutex> lock(storage_mutex);
        
        if (!is_initialized) {
            throw KMSException("LocalKMS is not initialized");
        }
        
        fs::path key_file = storage_path / (key + ".key");
        return fs::exists(key_file);
    }

    bool LocalKMS::DeleteSecret(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(storage_mutex);
        
        if (!is_initialized) {
            throw KMSException("LocalKMS is not initialized");
        }
        
        fs::path key_file = storage_path / (key + ".key");
        
        if (!fs::exists(key_file)) {
            return false;
        }
        
        try {
            // 🆕 삭제 전 쓰기 권한 복구
            fs::permissions(key_file, 
                fs::perms::owner_read | fs::perms::owner_write,
                fs::perm_options::replace);
            
            fs::remove(key_file);
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[LocalKMS] Failed to delete secret '" << key << "': " << e.what() << std::endl;
            return false;
        }
    }

} // namespace mpc_engine::kms