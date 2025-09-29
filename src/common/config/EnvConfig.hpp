// src/common/config/EnvConfig.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace mpc_engine::config
{
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
        bool LoadFromEnv(const std::string& env_name);  // env/.env.{env_name}

        // 설정값 조회
        std::string GetString(const std::string& key, const std::string& default_value = "") const;
        uint16_t GetUInt16(const std::string& key, uint16_t default_value = 0) const;
        uint32_t GetUInt32(const std::string& key, uint32_t default_value = 0) const;
        bool GetBool(const std::string& key, bool default_value = false) const;

        // 배열 형태 설정값 조회 (콤마로 구분된 값들)
        std::vector<std::string> GetStringArray(const std::string& key) const;
        std::vector<uint16_t> GetUInt16Array(const std::string& key) const;

        // 노드 엔드포인트 파싱 (host:port 형태)
        std::vector<std::pair<std::string, uint16_t>> GetNodeEndpoints(const std::string& key) const;

        // 설정값 존재 여부 확인
        bool HasKey(const std::string& key) const;

        // 환경 정보
        std::string GetEnvType() const { return env_type; }
        bool IsLoaded() const { return is_loaded; }

    private:
        // 파일 파싱 헬퍼
        bool ParseLine(const std::string& line);
    };
} // namespace mpc_engine::config