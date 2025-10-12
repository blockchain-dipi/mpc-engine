// tests/unit/https/test_https_library.cpp
#include "common/network/https/include/HttpsConnectionPool.hpp"
#include <iostream>
#include <cassert>

using namespace mpc_engine::https;

void PrintTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

// ========================================
// HttpsConnectionPool 테스트
// ========================================

bool TestConnectionPoolStats() {
    ConnectionPoolConfig config;
    config.max_connections = 10;
    config.min_idle = 2;
    
    HttpsConnectionPool pool(config);
    
    auto stats = pool.GetStats();
    assert(stats.total == 0);
    assert(stats.in_use == 0);
    assert(stats.idle == 0);
    
    return true;
}

bool TestConnectionPoolCloseAll() {
    ConnectionPoolConfig config;
    HttpsConnectionPool pool(config);
    
    // CloseAll 호출해도 crash 안 남
    pool.CloseAll();
    
    auto stats = pool.GetStats();
    assert(stats.total == 0);
    
    return true;
}

bool TestConnectionPoolConfig() {
    ConnectionPoolConfig config;
    config.max_connections = 20;
    config.min_idle = 5;
    config.max_idle_time_ms = 30000;
    
    HttpsConnectionPool pool(config);
    
    // 생성만 확인
    assert(true);
    
    return true;
}

// ========================================
// Main
// ========================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  HTTPS Library Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Note: HttpWriter/HttpResponseReader require real TLS connection" << std::endl;
    std::cout << "      Use manual testing or integration tests with real HTTPS server" << std::endl;
    std::cout << std::endl;
    
    try {
        // HttpsConnectionPool (TLS 연결 없이 테스트 가능)
        std::cout << "=== HttpsConnectionPool Tests ===" << std::endl;
        PrintTestResult("Stats initialization", TestConnectionPoolStats());
        PrintTestResult("CloseAll", TestConnectionPoolCloseAll());
        PrintTestResult("Config", TestConnectionPoolConfig());
        std::cout << std::endl;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  Basic tests passed!" << std::endl;
        std::cout << "  For full testing, use integration tests" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed: " << e.what() << std::endl;
        return 1;
    }
}