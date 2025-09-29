// tests/unit/threadpool_test.cpp
#include "common/utils/threading/ThreadPool.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cassert>
#include <vector>
#include <numeric>

using namespace mpc_engine::utils;
using namespace std::chrono_literals;

void PrintTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

// 1. 기본 작업 제출 및 실행 테스트
bool TestBasicSubmit() {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    
    // 10개 작업 제출
    for (int i = 0; i < 10; ++i) {
        pool.Submit([&counter]() {
            counter++;
        });
    }
    
    // 모든 작업이 완료될 때까지 대기
    std::this_thread::sleep_for(100ms);
    
    assert(counter == 10);
    assert(pool.GetThreadCount() == 4);
    
    return true;
}

// 2. 반환값이 있는 작업 테스트
bool TestSubmitTaskWithReturn() {
    ThreadPool pool(4);
    
    // Future 반환 작업
    auto future1 = pool.SubmitTask([]() { return 42; });
    auto future2 = pool.SubmitTask([](int a, int b) { return a + b; }, 10, 20);
    auto future3 = pool.SubmitTask([]() { 
        std::this_thread::sleep_for(50ms);
        return std::string("Hello"); 
    });
    
    assert(future1.get() == 42);
    assert(future2.get() == 30);
    assert(future3.get() == "Hello");
    
    return true;
}

// 3. 병렬 처리 테스트
bool TestParallelExecution() {
    ThreadPool pool(8);
    
    std::atomic<int> concurrent_count{0};
    std::atomic<int> max_concurrent{0};
    
    std::vector<std::future<void>> futures;
    
    // 16개 작업 제출 (각각 50ms 소요)
    for (int i = 0; i < 16; ++i) {
        auto future = pool.SubmitTask([&]() {
            int current = ++concurrent_count;
            
            // 최대 동시 실행 수 업데이트
            int expected = max_concurrent.load();
            while (current > expected && 
                   !max_concurrent.compare_exchange_weak(expected, current)) {
                expected = max_concurrent.load();
            }
            
            std::this_thread::sleep_for(50ms);
            concurrent_count--;
        });
        futures.push_back(std::move(future));
    }
    
    // 모든 작업 완료 대기
    for (auto& f : futures) {
        f.wait();
    }
    
    // 8개 워커로 최대 8개까지 동시 실행되어야 함
    std::cout << "  Max concurrent: " << max_concurrent << std::endl;
    assert(max_concurrent <= 8);
    assert(max_concurrent >= 4);  // 최소한 절반 이상은 동시 실행
    
    return true;
}

// 4. 예외 처리 테스트
bool TestExceptionHandling() {
    ThreadPool pool(4);
    
    std::atomic<int> success_count{0};
    
    // 정상 작업
    pool.Submit([&]() { success_count++; });
    
    // 예외 발생 작업
    pool.Submit([]() { 
        throw std::runtime_error("Test exception"); 
    });
    
    // 정상 작업
    pool.Submit([&]() { success_count++; });
    
    std::this_thread::sleep_for(100ms);
    
    // 예외가 발생해도 다른 작업은 정상 실행되어야 함
    assert(success_count == 2);
    
    // Future에서 예외 전파 테스트
    auto future = pool.SubmitTask([]() -> int {
        throw std::runtime_error("Task exception");
        return 42;
    });
    
    bool caught = false;
    try {
        future.get();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }
    
    assert(caught);
    
    return true;
}

// 5. 워커 수에 따른 성능 테스트
bool TestThreadScaling() {
    const int NUM_TASKS = 100;
    const int TASK_DURATION_MS = 10;
    
    auto RunTest = [&](size_t num_threads) {
        ThreadPool pool(num_threads);
        
        auto start = std::chrono::steady_clock::now();
        
        std::vector<std::future<void>> futures;
        for (int i = 0; i < NUM_TASKS; ++i) {
            auto future = pool.SubmitTask([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(TASK_DURATION_MS));
            });
            futures.push_back(std::move(future));
        }
        
        for (auto& f : futures) {
            f.wait();
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    };
    
    auto time_1_thread = RunTest(1);
    auto time_4_threads = RunTest(4);
    auto time_8_threads = RunTest(8);
    
    std::cout << "  1 thread:  " << time_1_thread << "ms" << std::endl;
    std::cout << "  4 threads: " << time_4_threads << "ms" << std::endl;
    std::cout << "  8 threads: " << time_8_threads << "ms" << std::endl;
    
    // 4 스레드가 1 스레드보다 빨라야 함
    assert(time_4_threads < time_1_thread);
    
    return true;
}

// 6. Shutdown 테스트
bool TestShutdown() {
    ThreadPool pool(4);
    
    std::atomic<int> completed{0};
    
    // 많은 작업 제출
    for (int i = 0; i < 100; ++i) {
        pool.Submit([&]() {
            std::this_thread::sleep_for(10ms);
            completed++;
        });
    }
    
    // 일부만 실행되고 Shutdown
    std::this_thread::sleep_for(50ms);
    pool.Shutdown();
    
    std::cout << "  Completed: " << completed << " / 100" << std::endl;
    assert(completed < 100);  // 전부 완료되진 않았어야 함
    assert(completed > 0);    // 일부는 완료되었어야 함
    assert(pool.IsStopped());
    
    // Shutdown 후 작업 제출 시 예외
    bool caught = false;
    try {
        pool.Submit([]() {});
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    
    return true;
}

// 7. 큐 오버플로우 테스트
bool TestQueueOverflow() {
    ThreadPool pool(2);  // 워커 2개
    
    std::atomic<bool> all_submitted{false};
    
    // 큐를 가득 채우는 스레드
    std::thread submitter([&]() {
        for (int i = 0; i < 1000; ++i) {
            pool.Submit([&]() {
                std::this_thread::sleep_for(10ms);
            });
        }
        all_submitted = true;
    });
    
    // 제출이 완료될 때까지 대기
    submitter.join();
    
    assert(all_submitted);
    std::cout << "  Successfully submitted 1000 tasks" << std::endl;
    
    return true;
}

// 8. 통계 테스트
bool TestStatistics() {
    ThreadPool pool(4);
    
    assert(pool.GetThreadCount() == 4);
    assert(pool.GetActiveTaskCount() == 0);
    assert(pool.GetPendingTaskCount() == 0);
    
    // 느린 작업 제출
    std::atomic<bool> can_proceed{false};
    
    for (int i = 0; i < 10; ++i) {
        pool.Submit([&]() {
            while (!can_proceed) {
                std::this_thread::sleep_for(10ms);
            }
        });
    }
    
    // 잠시 대기 후 통계 확인
    std::this_thread::sleep_for(50ms);
    
    size_t active = pool.GetActiveTaskCount();
    size_t pending = pool.GetPendingTaskCount();
    
    std::cout << "  Active tasks: " << active << std::endl;
    std::cout << "  Pending tasks: " << pending << std::endl;
    
    assert(active + pending == 10);
    assert(active <= 4);  // 워커 수 이하
    
    // 작업 완료 허용
    can_proceed = true;
    std::this_thread::sleep_for(100ms);
    
    assert(pool.GetActiveTaskCount() == 0);
    assert(pool.GetPendingTaskCount() == 0);
    
    return true;
}

// 9. 복잡한 작업 테스트
bool TestComplexTasks() {
    ThreadPool pool(8);
    
    // 재귀적 fibonacci
    std::function<int(int)> fib = [&](int n) -> int {
        if (n <= 1) return n;
        return fib(n - 1) + fib(n - 2);
    };
    
    std::vector<std::future<int>> futures;
    
    // 여러 fibonacci 계산 병렬 실행
    for (int i = 20; i <= 25; ++i) {
        auto future = pool.SubmitTask(fib, i);
        futures.push_back(std::move(future));
    }
    
    // 결과 검증
    std::vector<int> expected = {6765, 10946, 17711, 28657, 46368, 75025};
    for (size_t i = 0; i < futures.size(); ++i) {
        assert(futures[i].get() == expected[i]);
    }
    
    return true;
}

// 10. 성능 벤치마크
void TestPerformance() {
    const int NUM_TASKS = 10000;
    
    ThreadPool pool(8);
    
    auto start = std::chrono::steady_clock::now();
    
    std::vector<std::future<int>> futures;
    for (int i = 0; i < NUM_TASKS; ++i) {
        auto future = pool.SubmitTask([i]() { return i * 2; });
        futures.push_back(std::move(future));
    }
    
    int sum = 0;
    for (auto& f : futures) {
        sum += f.get();
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    std::cout << "[PERF] " << NUM_TASKS << " tasks in " << ms << "ms"
              << " (" << (NUM_TASKS * 1000.0 / ms) << " tasks/sec)" << std::endl;
    std::cout << "       Sum: " << sum << std::endl;
}

int main() {
    std::cout << "=== ThreadPool Unit Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        PrintTestResult("Basic Submit", TestBasicSubmit());
        PrintTestResult("Submit with Return", TestSubmitTaskWithReturn());
        PrintTestResult("Parallel Execution", TestParallelExecution());
        PrintTestResult("Exception Handling", TestExceptionHandling());
        PrintTestResult("Thread Scaling", TestThreadScaling());
        PrintTestResult("Shutdown", TestShutdown());
        PrintTestResult("Queue Overflow", TestQueueOverflow());
        PrintTestResult("Statistics", TestStatistics());
        PrintTestResult("Complex Tasks", TestComplexTasks());
        
        std::cout << std::endl;
        TestPerformance();
        
        std::cout << std::endl;
        std::cout << "=== All Tests Passed ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}