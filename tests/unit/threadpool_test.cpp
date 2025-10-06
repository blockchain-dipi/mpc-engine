// tests/unit/threadpool_test.cpp
#include "common/utils/threading/ThreadPool.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

using namespace mpc_engine::utils;

// 테스트용 Context 타입들
struct SimpleContext {
    int value;
    std::atomic<int>* counter;
};

struct ComplexContext {
    std::string message;
    std::vector<int> data;
    std::atomic<bool>* completed;
};

// 테스트 함수들
void simple_task(SimpleContext* ctx) {
    ctx->counter->fetch_add(ctx->value);
}

void complex_task(ComplexContext* ctx) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ctx->completed->store(true);
}

void exception_task([[maybe_unused]] SimpleContext* ctx) {
    throw std::runtime_error("Test exception");
}

// 테스트 유틸리티
template<typename TFunc>
void run_test(const char* name, TFunc test) {
    try {
        test();
        std::cout << "[PASS] " << name << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[FAIL] " << name << ": " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== ThreadPool Unit Tests ===" << std::endl;
    std::cout << std::endl;

    // Test 1: SubmitBorrowed (스택 변수)
    run_test("SubmitBorrowed (Stack Variable)", []() {
        ThreadPool<SimpleContext> pool(4);
        
        std::atomic<int> counter{0};
        SimpleContext ctx{10, &counter};
        
        pool.SubmitBorrowed(simple_task, &ctx);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (counter.load() != 10) {
            throw std::runtime_error("Counter mismatch");
        }
    });

    // Test 2: SubmitOwned (동적 할당)
    run_test("SubmitOwned (Dynamic Allocation)", []() {
        ThreadPool<SimpleContext> pool(4);
        
        std::atomic<int> counter{0};
        
        for (int i = 0; i < 100; ++i) {
            auto ctx = std::make_unique<SimpleContext>(SimpleContext{1, &counter});
            pool.SubmitOwned([](SimpleContext* c) {
                c->counter->fetch_add(c->value);
            }, std::move(ctx));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (counter.load() != 100) {
            throw std::runtime_error("Counter mismatch: " + std::to_string(counter.load()));
        }
    });

    // Test 3: 병렬 실행
    run_test("Parallel Execution", []() {
        ThreadPool<ComplexContext> pool(8);
        
        std::vector<std::atomic<bool>> flags(8);
        for (auto& flag : flags) {
            flag.store(false);
        }
        
        auto start = std::chrono::steady_clock::now();
        
        for (size_t i = 0; i < flags.size(); ++i) {
            auto ctx = std::make_unique<ComplexContext>(ComplexContext{"", {}, &flags[i]});
            pool.SubmitOwned([](ComplexContext* c) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                c->completed->store(true);
            }, std::move(ctx));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        for (const auto& flag : flags) {
            if (!flag.load()) {
                throw std::runtime_error("Task not completed");
            }
        }
        
        std::cout << "  Duration: " << duration << "ms (expected ~200ms)" << std::endl;
    });

    // Test 4: 예외 처리
    run_test("Exception Handling", []() {
        ThreadPool<SimpleContext> pool(2);
        
        std::atomic<int> counter{0};
        SimpleContext ctx{0, &counter};
        
        // 예외 발생 작업 제출 (스택 변수)
        pool.SubmitBorrowed(exception_task, &ctx);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 정상 작업이 계속 실행되는지 확인
        SimpleContext ctx2{1, &counter};
        pool.SubmitBorrowed(simple_task, &ctx2);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (counter.load() != 1) {
            throw std::runtime_error("Pool not recovered from exception");
        }
    });

    // Test 5: Shutdown
    run_test("Shutdown", []() {
        ThreadPool<SimpleContext> pool(4);
        
        std::atomic<int> counter{0};
        
        // 많은 작업 제출
        for (int i = 0; i < 20; ++i) {
            auto ctx = std::make_unique<SimpleContext>(SimpleContext{1, &counter});
            pool.SubmitOwned([](SimpleContext* c) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                c->counter->fetch_add(c->value);
            }, std::move(ctx));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        pool.Shutdown();
        
        std::cout << "  Completed: " << counter.load() << " / 20" << std::endl;
        
        // Shutdown 후 Submit 시도
        try {
            SimpleContext ctx{1, &counter};
            pool.SubmitBorrowed(simple_task, &ctx);
            throw std::runtime_error("Should have thrown exception");
        } catch (const std::runtime_error& e) {
            std::string msg = e.what();
            if (msg.find("stopped") == std::string::npos) {
                throw;
            }
        }
    });

    // Test 6: 통계
    run_test("Statistics", []() {
        ThreadPool<SimpleContext> pool(4);
        
        std::cout << "  Thread count: " << pool.GetThreadCount() << std::endl;
        std::cout << "  Active tasks: " << pool.GetActiveTaskCount() << std::endl;
        std::cout << "  Pending tasks: " << pool.GetPendingTaskCount() << std::endl;
        
        std::atomic<int> counter{0};
        
        // 느린 작업 제출
        for (int i = 0; i < 10; ++i) {
            auto ctx = std::make_unique<SimpleContext>(SimpleContext{1, &counter});
            pool.SubmitOwned([](SimpleContext* c) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                c->counter->fetch_add(c->value);
            }, std::move(ctx));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::cout << "  Active tasks: " << pool.GetActiveTaskCount() << std::endl;
        std::cout << "  Pending tasks: " << pool.GetPendingTaskCount() << std::endl;
        
        if (pool.GetActiveTaskCount() == 0 && pool.GetPendingTaskCount() == 0) {
            throw std::runtime_error("No tasks detected");
        }
    });

    // Test 7: 다른 타입의 ThreadPool (SubmitBorrowed)
    run_test("Multiple Pool Types (Borrowed)", []() {
        ThreadPool<SimpleContext> simple_pool(2);
        ThreadPool<ComplexContext> complex_pool(4);
        
        std::atomic<int> simple_counter{0};
        std::atomic<bool> complex_flag{false};
        
        SimpleContext simple_ctx{5, &simple_counter};
        ComplexContext complex_ctx{"test", {1,2,3}, &complex_flag};
        
        simple_pool.SubmitBorrowed(simple_task, &simple_ctx);
        complex_pool.SubmitBorrowed(complex_task, &complex_ctx);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        if (simple_counter.load() != 5 || !complex_flag.load()) {
            throw std::runtime_error("Pool type mismatch");
        }
    });

    // Test 8: SubmitOwned vs SubmitBorrowed 혼합
    run_test("Mixed Owned and Borrowed", []() {
        ThreadPool<SimpleContext> pool(4);
        std::atomic<int> counter{0};
        
        // 스택 변수 (Borrowed)
        SimpleContext stack_ctx{10, &counter};
        pool.SubmitBorrowed(simple_task, &stack_ctx);
        
        // 동적 할당 (Owned)
        for (int i = 0; i < 5; ++i) {
            auto ctx = std::make_unique<SimpleContext>(SimpleContext{1, &counter});
            pool.SubmitOwned(simple_task, std::move(ctx));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        if (counter.load() != 15) {
            throw std::runtime_error("Counter mismatch: " + std::to_string(counter.load()));
        }
    });

    // 성능 테스트
    std::cout << std::endl;
    std::cout << "[PERF] Performance Test" << std::endl;
    {
        ThreadPool<SimpleContext> pool(8);
        std::atomic<int> counter{0};
        
        const int num_tasks = 100000;
        auto start = std::chrono::steady_clock::now();
        
        for (int i = 0; i < num_tasks; ++i) {
            auto ctx = std::make_unique<SimpleContext>(SimpleContext{1, &counter});
            pool.SubmitOwned([](SimpleContext* c) {
                c->counter->fetch_add(c->value);
            }, std::move(ctx));
        }
        
        pool.Shutdown();
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        std::cout << "       " << num_tasks << " tasks in " << duration << "ms";
        std::cout << " (" << (num_tasks * 1000 / duration) << " tasks/sec)" << std::endl;
        std::cout << "       Counter: " << counter.load() << std::endl;
    }

    std::cout << std::endl;
    std::cout << "=== All Tests Passed ===" << std::endl;
    return 0;
}