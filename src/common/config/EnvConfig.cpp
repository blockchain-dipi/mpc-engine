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

    // ========================================
    // 모든 값은 필수 (없으면 예외)
    // ========================================

    std::string EnvConfig::GetString(const std::string& key) const 
    {
        auto it = config_map.find(key);
        if (it == config_map.end() || it->second.empty()) {
            throw ConfigMissingException(key);
        }
        return it->second;
    }

    uint16_t EnvConfig::GetUInt16(const std::string& key) const 
    {
        std::string value = GetString(key);  // 예외 발생 가능
        
        try {
            unsigned long parsed = std::stoul(value);
            if (parsed > UINT16_MAX) {
                throw std::out_of_range("Value too large for uint16_t");
            }
            return static_cast<uint16_t>(parsed);
        } catch (const std::exception& e) {
            throw std::runtime_error("Invalid uint16 value for key '" + key + "': " + value);
        }
    }

    uint32_t EnvConfig::GetUInt32(const std::string& key) const 
    {
        std::string value = GetString(key);  // 예외 발생 가능
        
        try {
            return static_cast<uint32_t>(std::stoul(value));
        } catch (const std::exception& e) {
            throw std::runtime_error("Invalid uint32 value for key '" + key + "': " + value);
        }
    }

    bool EnvConfig::GetBool(const std::string& key) const 
    {
        std::string value = GetString(key);  // 예외 발생 가능
        
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            return true;
        } else if (value == "false" || value == "0" || value == "no" || value == "off") {
            return false;
        } else {
            throw std::runtime_error("Invalid boolean value for key '" + key + "': " + value);
        }
    }

    std::vector<std::string> EnvConfig::GetStringArray(const std::string& key) const 
    {
        std::string value = GetString(key);  // 예외 발생 가능 (빈 문자열도 예외)

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
            
            if (!item.empty()) {
                result.push_back(item);
            }
        }
        
        if (result.empty()) {
            throw ConfigMissingException(key + " (array is empty)");
        }
        
        return result;
    }

    std::vector<uint16_t> EnvConfig::GetUInt16Array(const std::string& key) const 
    {
        std::vector<std::string> str_array = GetStringArray(key);  // 예외 발생 가능
        std::vector<uint16_t> result;

        for (const std::string& str : str_array)
        {
            try {
                unsigned long parsed = std::stoul(str);
                if (parsed > UINT16_MAX) {
                    throw std::out_of_range("Value too large");
                }
                result.push_back(static_cast<uint16_t>(parsed));
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid uint16 value in array '" + key + "': " + str);
            }
        }
        
        return result;
    }

    std::vector<std::pair<std::string, uint16_t>> EnvConfig::GetNodeEndpoints(const std::string& key) const 
    {
        std::vector<std::string> str_array = GetStringArray(key);  // 예외 발생 가능
        std::vector<std::pair<std::string, uint16_t>> result;

        for (const std::string& endpoint : str_array)
        {
            size_t colon_pos = endpoint.find(':');
            if (colon_pos == std::string::npos) {
                throw std::runtime_error("Invalid endpoint format in '" + key + "': " + endpoint + " (expected host:port)");
            }
            
            std::string host = endpoint.substr(0, colon_pos);
            std::string port_str = endpoint.substr(colon_pos + 1);
            
            // 앞뒤 공백 제거
            host.erase(0, host.find_first_not_of(" \t"));
            host.erase(host.find_last_not_of(" \t") + 1);
            port_str.erase(0, port_str.find_first_not_of(" \t"));
            port_str.erase(port_str.find_last_not_of(" \t") + 1);
            
            if (host.empty()) {
                throw std::runtime_error("Empty host in endpoint '" + key + "': " + endpoint);
            }
            
            try {
                unsigned long parsed = std::stoul(port_str);
                if (parsed == 0 || parsed > UINT16_MAX) {
                    throw std::out_of_range("Port out of range");
                }
                uint16_t port = static_cast<uint16_t>(parsed);
                result.emplace_back(host, port);
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid port in endpoint '" + key + "': " + endpoint);
            }
        }
        
        return result;
    }

    bool EnvConfig::HasKey(const std::string& key) const 
    {
        return config_map.find(key) != config_map.end();
    }

    void EnvConfig::ValidateRequired(const std::vector<std::string>& required_keys) const 
    {
        std::vector<std::string> missing_keys;
        
        for (const std::string& key : required_keys) {
            if (!HasKey(key) || config_map.at(key).empty()) {
                missing_keys.push_back(key);
            }
        }
        
        if (!missing_keys.empty()) {
            std::stringstream ss;
            ss << "Missing required configuration keys: ";
            for (size_t i = 0; i < missing_keys.size(); ++i) {
                ss << missing_keys[i];
                if (i < missing_keys.size() - 1) {
                    ss << ", ";
                }
            }
            throw std::runtime_error(ss.str());
        }
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