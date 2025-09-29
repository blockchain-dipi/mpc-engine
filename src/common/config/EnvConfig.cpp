// src/common/config/EnvConfig.cpp
#include "EnvConfig.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace mpc_engine::config
{
    bool EnvConfig::LoadFromFile(const std::string& file_path) 
    {
        std::ifstream file(file_path);
        if (!file.is_open()) 
        {
            std::cerr << "Failed to open config file: " << file_path << std::endl;
            return false;
        }
        config_map.clear();

        std::string line;
        while (std::getline(file, line)) 
        {
            ParseLine(line);
        }
        file.close();

        is_loaded = true;
        std::cout << "Loaded " << config_map.size() << " configuration entries from " << file_path << std::endl;
        return true;
    }

    bool EnvConfig::LoadFromEnv(const std::string& env_name) 
    {
        env_type = env_name;
        std::string file_path = "env/.env." + env_name;
        return LoadFromFile(file_path);
    }

    std::string EnvConfig::GetString(const std::string& key, const std::string& default_value) const 
    {
        std::unordered_map<std::string, std::string>::const_iterator it = config_map.find(key);
        return (it != config_map.end()) ? it->second : default_value;
    }

    uint16_t EnvConfig::GetUInt16(const std::string& key, uint16_t default_value) const 
    {
        std::string value = GetString(key);
        if (value.empty()) 
        {
            return default_value;
        }
        try 
        {
            return static_cast<uint16_t>(std::stoul(value));
        }
        catch (...) 
        {
            return default_value;
        }
    }

    uint32_t EnvConfig::GetUInt32(const std::string& key, uint32_t default_value) const 
    {
        std::string value = GetString(key);
        if (value.empty()) 
        {
            return default_value;
        }
        try 
        {
            return static_cast<uint32_t>(std::stoul(value));
        }
        catch (...) 
        {
            return default_value;
        }
    }

    bool EnvConfig::GetBool(const std::string& key, bool default_value) const 
    {
        std::string value = GetString(key);
        if (value.empty())
        {
            return default_value;
        }
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == "true" || value == "1" || value == "yes" || value == "on");
    }

    std::vector<std::string> EnvConfig::GetStringArray(const std::string& key) const 
    {
        std::string value = GetString(key);
        if (value.empty())
        {
            return {};
        }

        std::vector<std::string> result;
        std::stringstream ss(value);
        std::string item;
        while (std::getline(ss, item, ',')) 
        {
            // 앞뒤 공백 제거
            size_t start = item.find_first_not_of(" \t");
            if (start != std::string::npos) {
                size_t end = item.find_last_not_of(" \t");
                item = item.substr(start, end - start + 1);
            } else {
                item.clear();
            }
            if (!item.empty()) 
            {
                result.push_back(item);
            }
        }
        return result;
    }

    std::vector<uint16_t> EnvConfig::GetUInt16Array(const std::string& key) const 
    {
        std::vector<std::string> str_array = GetStringArray(key);
        std::vector<uint16_t> result;

        for (const std::string& str : str_array)
        {
            try 
            {
                result.push_back(static_cast<uint16_t>(std::stoul(str)));
            }
            catch (...) 
            {
                result.push_back(0);
            }
        }
        return result;
    }

    std::vector<std::pair<std::string, uint16_t>> EnvConfig::GetNodeEndpoints(const std::string& key) const 
    {
        std::vector<std::string> str_array = GetStringArray(key);
        std::vector<std::pair<std::string, uint16_t>> result;

        for (const std::string& endpoint : str_array)
        {
            size_t colon_pos = endpoint.find(':');
            if (colon_pos != std::string::npos) 
            {
                std::string host = endpoint.substr(0, colon_pos);
                std::string port_str = endpoint.substr(colon_pos + 1);
                // 앞뒤 공백 제거
                host.erase(0, host.find_first_not_of(" \t"));
                host.erase(host.find_last_not_of(" \t") + 1);
                port_str.erase(0, port_str.find_first_not_of(" \t"));
                port_str.erase(port_str.find_last_not_of(" \t") + 1);
                try 
                {
                    uint16_t port = static_cast<uint16_t>(std::stoul(port_str));
                    if (!host.empty() && port > 0) {
                        result.emplace_back(host, port);
                    }
                }
                catch (...) 
                {
                    // 파싱 실패 시 무시
                }
            }
        }
        return result;
    }

    bool EnvConfig::HasKey(const std::string& key) const 
    {
        return config_map.find(key) != config_map.end();
    }

    bool EnvConfig::ParseLine(const std::string& line) 
    {
        // 앞뒤 공백 제거
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

        // 빈 줄이나 주석은 무시
        if (trimmed.empty() || trimmed[0] == '#') 
        {
            return true;
        }
        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) 
        {
            return false;
        }

        std::string key = trimmed.substr(0, eq_pos);
        std::string value = trimmed.substr(eq_pos + 1);
        // 앞뒤 공백 제거
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (!key.empty()) 
        {
            config_map[key] = value;
            return true;
        }
        return false;
    }
} // namespace mpc_engine::config