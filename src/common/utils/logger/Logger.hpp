// src/common/utils/logger/Logger.hpp
#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <memory>
#include <cstdarg>
#include <cstring>

#ifndef COMPILE_LOG_LEVEL
    #ifdef NDEBUG
        #define COMPILE_LOG_LEVEL 3  // Release: ERROR 이상만
    #else
        #define COMPILE_LOG_LEVEL 0  // Debug: 모든 로그
    #endif
#endif

namespace mpc_engine::utils {

enum class LogLevel : int {
    DEBUG = 0,  // 개발 디버깅
    INFO = 1,   // 주요 이벤트
    WARN = 2,   // 경고
    ERROR = 3,  // 오류
    FATAL = 4,  // 치명적 오류
    NONE = 5    // 로그 비활성화
};

class Logger {
private:
    inline static std::unique_ptr<Logger> instance = nullptr;
    inline static std::mutex instance_mutex;
    
    LogLevel min_level = LogLevel::INFO;
    std::mutex log_mutex;
    std::ofstream file;
    bool console_enabled = true;
    bool file_enabled = false;
    
    Logger() = default;
    
    static const char* LogLevelToString(LogLevel level) {
        switch(level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "UNKN ";
        }
    }
    
    static LogLevel StringToLogLevel(const char* str) {
        if (!str) return LogLevel::INFO;
        
        std::string level_str(str);
        if (level_str == "DEBUG" || level_str == "0") return LogLevel::DEBUG;
        if (level_str == "INFO"  || level_str == "1") return LogLevel::INFO;
        if (level_str == "WARN"  || level_str == "2") return LogLevel::WARN;
        if (level_str == "ERROR" || level_str == "3") return LogLevel::ERROR;
        if (level_str == "FATAL" || level_str == "4") return LogLevel::FATAL;
        if (level_str == "NONE"  || level_str == "5") return LogLevel::NONE;
        
        return LogLevel::INFO;
    }
    
    static std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
public:
    static Logger& Instance() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (!instance) {
            instance.reset(new Logger());
        }
        return *instance;
    }
    
    void Initialize(const char* log_file = nullptr, bool enable_console = true) {
        console_enabled = enable_console;
        
        if (log_file) {
            file.open(log_file, std::ios::app);
            file_enabled = file.is_open();
        }
        
        const char* runtime_level = std::getenv("RUNTIME_LOG_LEVEL");
        if (!runtime_level) {
            runtime_level = std::getenv("LOG_LEVEL");
        }
        
        if (runtime_level) {
            LogLevel requested_level = StringToLogLevel(runtime_level);
            
            if (static_cast<int>(requested_level) < COMPILE_LOG_LEVEL) {
                std::cerr << "[Logger] Warning: RUNTIME_LOG_LEVEL(" 
                         << static_cast<int>(requested_level)
                         << ") < COMPILE_LOG_LEVEL(" << COMPILE_LOG_LEVEL 
                         << "). Using COMPILE_LOG_LEVEL." << std::endl;
                min_level = static_cast<LogLevel>(COMPILE_LOG_LEVEL);
            } else {
                min_level = requested_level;
            }
        } else {
            min_level = static_cast<LogLevel>(COMPILE_LOG_LEVEL);
        }
        
        if (console_enabled) {
            std::cout << "[Logger] Initialized" << std::endl;
            std::cout << "  Compile Level: " << COMPILE_LOG_LEVEL 
                     << " (" << LogLevelToString(static_cast<LogLevel>(COMPILE_LOG_LEVEL)) << ")" << std::endl;
            std::cout << "  Runtime Level: " << static_cast<int>(min_level)
                     << " (" << LogLevelToString(min_level) << ")" << std::endl;
            if (file_enabled) {
                std::cout << "  Log File: " << log_file << std::endl;
            }
        }
    }
    
    void Log(LogLevel level, const char* category, const char* message) {
        if (static_cast<int>(level) < static_cast<int>(min_level)) return;
        
        std::string log_line = GetTimestamp() + " [" + LogLevelToString(level) + "] " +
                               "[" + category + "] " + message + "\n";
        
        std::lock_guard<std::mutex> lock(log_mutex);
        
        if (console_enabled) {
            if (level >= LogLevel::ERROR) {
                std::cerr << log_line;
            } else {
                std::cout << log_line;
            }
        }
        
        if (file_enabled && file.is_open()) {
            file << log_line;
            if (level >= LogLevel::ERROR) {
                file.flush();
            }
        }
    }
    
    void Logf(LogLevel level, const char* category, const char* format, ...) {
        if (static_cast<int>(level) < static_cast<int>(min_level)) return;
        
        char buffer[4096];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        Log(level, category, buffer);
    }
    
    ~Logger() {
        if (file.is_open()) {
            file.close();
        }
    }
};

} // namespace mpc_engine::utils

// ========================================
// 컴파일 타임 로그 제거 매크로
// ========================================

#define LOG_IMPL(level, cat, msg) \
    mpc_engine::utils::Logger::Instance().Log(mpc_engine::utils::LogLevel::level, cat, msg)

#define LOG_IMPLF(level, cat, fmt, ...) \
    mpc_engine::utils::Logger::Instance().Logf(mpc_engine::utils::LogLevel::level, cat, fmt, ##__VA_ARGS__)

// DEBUG (COMPILE_LOG_LEVEL <= 0)
#if COMPILE_LOG_LEVEL <= 0
    #define LOG_DEBUG(cat, msg) LOG_IMPL(DEBUG, cat, msg)
    #define LOG_DEBUGF(cat, fmt, ...) LOG_IMPLF(DEBUG, cat, fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(cat, msg) ((void)0)
    #define LOG_DEBUGF(cat, fmt, ...) ((void)0)
#endif

// INFO (COMPILE_LOG_LEVEL <= 1)
#if COMPILE_LOG_LEVEL <= 1
    #define LOG_INFO(cat, msg) LOG_IMPL(INFO, cat, msg)
    #define LOG_INFOF(cat, fmt, ...) LOG_IMPLF(INFO, cat, fmt, ##__VA_ARGS__)
#else
    #define LOG_INFO(cat, msg) ((void)0)
    #define LOG_INFOF(cat, fmt, ...) ((void)0)
#endif

// WARN (COMPILE_LOG_LEVEL <= 2)
#if COMPILE_LOG_LEVEL <= 2
    #define LOG_WARN(cat, msg) LOG_IMPL(WARN, cat, msg)
    #define LOG_WARNF(cat, fmt, ...) LOG_IMPLF(WARN, cat, fmt, ##__VA_ARGS__)
#else
    #define LOG_WARN(cat, msg) ((void)0)
    #define LOG_WARNF(cat, fmt, ...) ((void)0)
#endif

// ERROR (COMPILE_LOG_LEVEL <= 3)
#if COMPILE_LOG_LEVEL <= 3
    #define LOG_ERROR(cat, msg) LOG_IMPL(ERROR, cat, msg)
    #define LOG_ERRORF(cat, fmt, ...) LOG_IMPLF(ERROR, cat, fmt, ##__VA_ARGS__)
#else
    #define LOG_ERROR(cat, msg) ((void)0)
    #define LOG_ERRORF(cat, fmt, ...) ((void)0)
#endif

// FATAL (COMPILE_LOG_LEVEL <= 4)
#if COMPILE_LOG_LEVEL <= 4
    #define LOG_FATAL(cat, msg) LOG_IMPL(FATAL, cat, msg)
    #define LOG_FATALF(cat, fmt, ...) LOG_IMPLF(FATAL, cat, fmt, ##__VA_ARGS__)
#else
    #define LOG_FATAL(cat, msg) ((void)0)
    #define LOG_FATALF(cat, fmt, ...) ((void)0)
#endif