// src/common/resource/src/LocalReadOnlyResLoader.cpp
#include "common/resource/include/LocalReadOnlyResLoader.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace mpc_engine::resource
{
    LocalReadOnlyResLoader::LocalReadOnlyResLoader()
        : initialized_(true)
    {
        // 로컬 파일 시스템은 특별한 초기화 불필요
    }

    std::string LocalReadOnlyResLoader::ReadFile(const std::string& path)
    {
        std::string normalized_path = NormalizePath(path);

        if (!Exists(normalized_path))
        {
            throw std::runtime_error("Resource not found: " + normalized_path);
        }

        std::ifstream file(normalized_path, std::ios::in);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open resource: " + normalized_path);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        
        if (file.bad())
        {
            throw std::runtime_error("Failed to read resource: " + normalized_path);
        }

        return buffer.str();
    }

    std::vector<uint8_t> LocalReadOnlyResLoader::ReadBinaryFile(const std::string& path)
    {
        std::string normalized_path = NormalizePath(path);

        if (!Exists(normalized_path))
        {
            throw std::runtime_error("Resource not found: " + normalized_path);
        }

        std::ifstream file(normalized_path, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open resource: " + normalized_path);
        }

        // 파일 크기 확인
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size < 0)
        {
            throw std::runtime_error("Failed to get resource size: " + normalized_path);
        }

        // 데이터 읽기
        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (size > 0)
        {
            if (!file.read(reinterpret_cast<char*>(data.data()), size))
            {
                throw std::runtime_error("Failed to read binary resource: " + normalized_path);
            }
        }

        return data;
    }

    bool LocalReadOnlyResLoader::Exists(const std::string& path)
    {
        try
        {
            std::string normalized_path = NormalizePath(path);
            return fs::exists(normalized_path) && fs::is_regular_file(normalized_path);
        }
        catch (const fs::filesystem_error&)
        {
            return false;
        }
    }

    bool LocalReadOnlyResLoader::IsInitialized() const
    {
        return initialized_;
    }

    std::string LocalReadOnlyResLoader::NormalizePath(const std::string& path) const
    {
        try
        {
            fs::path p(path);
            
            // 절대 경로면 그대로 반환
            if (p.is_absolute())
            {
                return p.string();
            }
            
            // 상대 경로면 현재 작업 디렉토리 기준으로 변환
            fs::path absolute_path = fs::absolute(p);
            return absolute_path.string();
        }
        catch (const fs::filesystem_error& e)
        {
            throw std::runtime_error("Failed to normalize path: " + path + " - " + e.what());
        }
    }

} // namespace mpc_engine::resource