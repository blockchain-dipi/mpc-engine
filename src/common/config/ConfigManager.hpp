// src/common/config/ConfigManager.hpp
#pragma once
#include "EnvConfig.hpp"
#include <memory>
#include <mutex>
#include <string>

namespace mpc_engine::config
{
    /**
     * @brief 글로벌 설정 관리자 (싱글톤)
     * 
     * 전체 애플리케이션에서 하나의 설정 인스턴스를 공유하여
     * 일관성과 메모리 효율성을 보장합니다.
     */
    class ConfigManager 
    {
    private:
        static std::unique_ptr<ConfigManager> instance;
        static std::mutex instance_mutex;
        
        std::unique_ptr<EnvConfig> env_config;
        mutable std::mutex config_mutex;
        
        bool is_initialized = false;
        
        // 생성자를 private으로 하여 외부 생성 방지
        ConfigManager() = default;

    public:
        ~ConfigManager() = default;
        
        // 복사/이동 방지
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;
        ConfigManager(ConfigManager&&) = delete;
        ConfigManager& operator=(ConfigManager&&) = delete;

        /**
         * @brief 싱글톤 인스턴스 획득
         */
        static ConfigManager& Instance();

        /**
         * @brief 환경 설정 초기화
         * @param env_type 환경 타입 (local, dev, qa, production)
         * @return 초기화 성공 여부
         */
        bool Initialize(const std::string& env_type);

        /**
         * @brief 초기화 상태 확인
         */
        bool IsInitialized() const;

        /**
         * @brief 환경 설정 객체 접근
         * @return EnvConfig 참조 (초기화되지 않았으면 예외 발생)
         */
        const EnvConfig& GetConfig() const;

        /**
         * @brief 편의 메서드들 - EnvConfig 메서드를 직접 호출
         */
        std::string GetString(const std::string& key) const;
        uint16_t GetUInt16(const std::string& key) const;
        uint32_t GetUInt32(const std::string& key) const;
        bool GetBool(const std::string& key) const;
        std::vector<std::string> GetStringArray(const std::string& key) const;
        std::vector<uint16_t> GetUInt16Array(const std::string& key) const;
        std::vector<std::pair<std::string, uint16_t>> GetNodeEndpoints(const std::string& key) const;
        bool HasKey(const std::string& key) const;
        std::string GetEnvType() const;

        /**
         * @brief 필수 키 검증
         */
        void ValidateRequired(const std::vector<std::string>& required_keys) const;

        /**
         * @brief 설정 재로드 (런타임 중 설정 변경 시 사용)
         */
        bool Reload();

        /**
         * @brief 현재 로드된 설정 정보 출력 (디버깅용)
         */
        void PrintLoadedConfig() const;

    private:
        void EnsureInitialized() const;
    };

    /**
     * @brief 전역 설정 접근을 위한 편의 함수들
     * 
     * ConfigManager::Instance().GetString(key) 대신
     * Config::GetString(key)로 간단하게 사용 가능
     */
    namespace Config 
    {
        inline const EnvConfig& Get() {
            return ConfigManager::Instance().GetConfig();
        }

        inline std::string GetString(const std::string& key) {
            return ConfigManager::Instance().GetString(key);
        }

        inline uint16_t GetUInt16(const std::string& key) {
            return ConfigManager::Instance().GetUInt16(key);
        }

        inline uint32_t GetUInt32(const std::string& key) {
            return ConfigManager::Instance().GetUInt32(key);
        }

        inline bool GetBool(const std::string& key) {
            return ConfigManager::Instance().GetBool(key);
        }

        inline std::vector<std::string> GetStringArray(const std::string& key) {
            return ConfigManager::Instance().GetStringArray(key);
        }

        inline std::vector<uint16_t> GetUInt16Array(const std::string& key) {
            return ConfigManager::Instance().GetUInt16Array(key);
        }

        inline std::vector<std::pair<std::string, uint16_t>> GetNodeEndpoints(const std::string& key) {
            return ConfigManager::Instance().GetNodeEndpoints(key);
        }

        inline bool HasKey(const std::string& key) {
            return ConfigManager::Instance().HasKey(key);
        }

        inline std::string GetEnvType() {
            return ConfigManager::Instance().GetEnvType();
        }

        inline void ValidateRequired(const std::vector<std::string>& required_keys) {
            ConfigManager::Instance().ValidateRequired(required_keys);
        }

        inline bool IsInitialized() {
            return ConfigManager::Instance().IsInitialized();
        }
    }

} // namespace mpc_engine::config