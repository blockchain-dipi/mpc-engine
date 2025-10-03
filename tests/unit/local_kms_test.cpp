// tests/unit/local_kms_test.cpp
#include "common/kms/include/KMSFactory.hpp"
#include "common/kms/include/LocalKMS.hpp"
#include "common/kms/include/KMSException.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <thread>
#include <vector>
#include <atomic>

using namespace mpc_engine::kms;
namespace fs = std::filesystem;

void PrintTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

// Test 1: 기본 동작
bool TestBasicOperations() {
    std::cout << "\n=== Test 1: Basic Operations ===" << std::endl;
    
    std::string test_path = ".kms_test_basic";
    
    if (fs::exists(test_path)) {
        fs::remove_all(test_path);
    }
    
    auto kms = KMSFactory::Create("local", test_path);
    assert(kms->Initialize());
    assert(kms->IsInitialized());
    
    // PutSecret
    bool put_result = kms->PutSecret("test_key_1", "secret_value_123");
    assert(put_result);
    PrintTestResult("PutSecret", true);
    
    // SecretExists
    assert(kms->SecretExists("test_key_1"));
    PrintTestResult("SecretExists (existing)", true);
    
    assert(!kms->SecretExists("non_existent_key"));
    PrintTestResult("SecretExists (non-existing)", true);
    
    // GetSecret
    std::string value = kms->GetSecret("test_key_1");
    assert(value == "secret_value_123");
    PrintTestResult("GetSecret", true);
    
    // DeleteSecret
    bool delete_result = kms->DeleteSecret("test_key_1");
    assert(delete_result);
    assert(!kms->SecretExists("test_key_1"));
    PrintTestResult("DeleteSecret", true);
    
    fs::remove_all(test_path);
    return true;
}

// Test 2: 예외 처리
bool TestExceptions() {
    std::cout << "\n=== Test 2: Exception Handling ===" << std::endl;
    
    std::string test_path = ".kms_test_exceptions";
    
    if (fs::exists(test_path)) {
        fs::remove_all(test_path);
    }
    
    auto kms = KMSFactory::Create("local", test_path);
    assert(kms->Initialize());
    
    // GetSecret on non-existent key
    try {
        kms->GetSecret("non_existent_key");
        assert(false);
    } catch (const SecretNotFoundException& e) {
        std::string msg = e.what();
        assert(msg.find("non_existent_key") != std::string::npos);
        PrintTestResult("SecretNotFoundException", true);
    }
    
    // 초기화 전 사용 시도
    auto uninit_kms = std::make_unique<LocalKMS>(".kms_test_uninit");
    try {
        uninit_kms->PutSecret("key", "value");
        assert(false);
    } catch (const KMSException& e) {
        std::string msg = e.what();
        assert(msg.find("not initialized") != std::string::npos);
        PrintTestResult("Uninitialized KMS exception", true);
    }
    
    fs::remove_all(test_path);
    fs::remove_all(".kms_test_uninit");
    return true;
}

// Test 3: 여러 시크릿
bool TestMultipleSecrets() {
    std::cout << "\n=== Test 3: Multiple Secrets ===" << std::endl;
    
    std::string test_path = ".kms_test_multiple";
    
    if (fs::exists(test_path)) {
        fs::remove_all(test_path);
    }
    
    auto kms = KMSFactory::Create("local", test_path);
    assert(kms->Initialize());
    
    // 여러 시크릿 저장
    kms->PutSecret("key_1", "value_1");
    kms->PutSecret("key_2", "value_2");
    kms->PutSecret("key_3", "value_3");
    
    // 모두 존재 확인
    assert(kms->SecretExists("key_1"));
    assert(kms->SecretExists("key_2"));
    assert(kms->SecretExists("key_3"));
    
    // 모두 조회
    assert(kms->GetSecret("key_1") == "value_1");
    assert(kms->GetSecret("key_2") == "value_2");
    assert(kms->GetSecret("key_3") == "value_3");
    
    PrintTestResult("Multiple secrets", true);
    
    fs::remove_all(test_path);
    return true;
}

// Test 4: 큰 데이터
bool TestLargeData() {
    std::cout << "\n=== Test 4: Large Data ===" << std::endl;
    
    std::string test_path = ".kms_test_large";
    
    if (fs::exists(test_path)) {
        fs::remove_all(test_path);
    }
    
    auto kms = KMSFactory::Create("local", test_path);
    assert(kms->Initialize());
    
    // 큰 데이터 (100KB)
    std::string large_data(100000, 'X');
    kms->PutSecret("large_key", large_data);
    
    std::string retrieved = kms->GetSecret("large_key");
    assert(retrieved.size() == 100000);
    assert(retrieved == large_data);
    
    PrintTestResult("Large data (100KB)", true);
    
    fs::remove_all(test_path);
    return true;
}

// Test 5: Factory 테스트
bool TestFactory() {
    std::cout << "\n=== Test 5: Factory Pattern ===" << std::endl;
    
    // Valid providers
    std::vector<std::string> valid_providers = {"local", "LOCAL", "aws", "AWS", "azure", "ibm", "google"};
    
    for (const auto& provider : valid_providers) {
        try {
            auto kms = KMSFactory::Create(provider, ".kms_test_factory");
            if (provider == "local" || provider == "LOCAL") {
                assert(kms->Initialize());
                PrintTestResult("Factory create: " + provider, true);
            } else {
                // 클라우드 KMS는 Initialize 시 예외 발생
                try {
                    kms->Initialize();
                    assert(false);
                } catch (const std::runtime_error& e) {
                    std::string msg = e.what();
                    assert(msg.find("not implemented") != std::string::npos);
                    PrintTestResult("Factory create (stub): " + provider, true);
                }
            }
        } catch (...) {
            assert(false);
        }
    }
    
    // Invalid provider
    try {
        auto kms = KMSFactory::Create("invalid_provider");
        assert(false);
    } catch (const std::invalid_argument& e) {
        PrintTestResult("Invalid provider exception", true);
    }
    
    // IsValidProvider
    assert(KMSFactory::IsValidProvider("local"));
    assert(KMSFactory::IsValidProvider("AWS"));
    assert(!KMSFactory::IsValidProvider("invalid"));
    PrintTestResult("IsValidProvider", true);
    
    // GetSupportedProviders
    auto providers = KMSFactory::GetSupportedProviders();
    assert(providers.size() == 5);
    PrintTestResult("GetSupportedProviders", true);
    
    fs::remove_all(".kms_test_factory");
    return true;
}

// Test 6: 멀티스레드 안전성
bool TestThreadSafety() {
    std::cout << "\n=== Test 6: Thread Safety ===" << std::endl;
    
    std::string test_path = ".kms_test_threads";
    
    if (fs::exists(test_path)) {
        fs::remove_all(test_path);
    }
    
    auto kms = KMSFactory::Create("local", test_path);
    assert(kms->Initialize());
    
    const int NUM_THREADS = 10;
    const int OPS_PER_THREAD = 100;
    std::vector<std::thread> threads;
    
    // 동시 쓰기
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&kms, i]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                std::string value = "value_" + std::to_string(i) + "_" + std::to_string(j);
                kms->PutSecret(key, value);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    threads.clear();
    
    // 동시 읽기
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&kms, &success_count, i]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                std::string expected = "value_" + std::to_string(i) + "_" + std::to_string(j);
                
                try {
                    std::string value = kms->GetSecret(key);
                    if (value == expected) {
                        success_count++;
                    }
                } catch (...) {
                    // 예외 발생 시 실패
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int expected_total = NUM_THREADS * OPS_PER_THREAD;
    assert(success_count.load() == expected_total);
    PrintTestResult("Thread safety (" + std::to_string(expected_total) + " ops)", true);
    
    fs::remove_all(test_path);
    return true;
}

// Test 7: 파일 권한 (Linux only)
bool TestFilePermissions() {
    std::cout << "\n=== Test 7: File Permissions ===" << std::endl;
    
#ifdef __linux__
    std::string test_path = ".kms_test_perms";
    
    if (fs::exists(test_path)) {
        fs::remove_all(test_path);
    }
    
    auto kms = KMSFactory::Create("local", test_path);
    assert(kms->Initialize());
    
    kms->PutSecret("test_key", "test_value");
    
    std::string key_file = test_path + "/test_key.key";
    assert(fs::exists(key_file));
    
    // 파일 권한 확인 (400)
    auto perms = fs::status(key_file).permissions();
    bool owner_read = (perms & fs::perms::owner_read) != fs::perms::none;
    bool owner_write = (perms & fs::perms::owner_write) == fs::perms::none;
    bool group_read = (perms & fs::perms::group_read) == fs::perms::none;
    bool others_read = (perms & fs::perms::others_read) == fs::perms::none;
    
    assert(owner_read);
    assert(owner_write);
    assert(group_read);
    assert(others_read);
    
    PrintTestResult("File permissions (400)", true);
    
    fs::remove_all(test_path);
#else
    PrintTestResult("File permissions (skipped on non-Linux)", true);
#endif
    
    return true;
}

// Test 8: 값 덮어쓰기
bool TestOverwrite() {
    std::cout << "\n=== Test 8: Overwrite Secrets ===" << std::endl;
    
    std::string test_path = ".kms_test_overwrite";
    
    if (fs::exists(test_path)) {
        fs::remove_all(test_path);
    }
    
    auto kms = KMSFactory::Create("local", test_path);
    assert(kms->Initialize());
    
    // 첫 번째 값
    kms->PutSecret("key", "original_value");
    assert(kms->GetSecret("key") == "original_value");
    
    // 덮어쓰기
    kms->PutSecret("key", "new_value");
    assert(kms->GetSecret("key") == "new_value");
    
    PrintTestResult("Overwrite secret", true);
    
    fs::remove_all(test_path);
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Local KMS Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        TestBasicOperations();
        TestExceptions();
        TestMultipleSecrets();
        TestLargeData();
        TestFactory();
        TestThreadSafety();
        TestFilePermissions();
        TestOverwrite();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  All tests passed!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed: " << e.what() << std::endl;
        return 1;
    }
}