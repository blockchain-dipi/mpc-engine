// src/common/env/EnvConfig.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace mpc_engine::env
{
    // 설정 누락 예외
    class ConfigMissingException : public std::runtime_error {
    public:
        explicit ConfigMissingException(const std::string& key) 
            : std::runtime_error("Required config missing: " + key) {}
    };

    class EnvConfig 
    {
    private:
        std::unordered_map<std::string, std::string> config_map;
        std::string env_type;
        bool is_loaded = false;

    public:
        EnvConfig() = default;
        ~EnvConfig() = default;

        // 환경 설정 파일 로드
        bool LoadFromFile(const std::string& file_path);
        bool LoadFromEnv(const std::string& env_name);

        // 모든 값은 필수 (없으면 예외 발생)
        std::string GetString(const std::string& key) const;
        uint16_t GetUInt16(const std::string& key) const;
        uint32_t GetUInt32(const std::string& key) const;
        bool GetBool(const std::string& key) const;

        // 배열 형태 설정값 조회 (모두 필수)
        std::vector<std::string> GetStringArray(const std::string& key) const;
        std::vector<uint16_t> GetUInt16Array(const std::string& key) const;
        std::vector<std::pair<std::string, uint16_t>> GetNodeEndpoints(const std::string& key) const;

        // 설정값 존재 여부 확인
        bool HasKey(const std::string& key) const;

        // 환경 정보
        std::string GetEnvType() const { return env_type; }
        bool IsLoaded() const { return is_loaded; }

        // 여러 필수 키 한번에 검증
        void ValidateRequired(const std::vector<std::string>& required_keys) const;

    private:
        bool ParseLine(const std::string& line);
    };
} // namespace mpc_engine::env