// tests/unit/threadsafe_queue_test.cpp
#include "common/utils/queue/ThreadSafeQueue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>

using namespace mpc_engine::utils;
using namespace std::chrono_literals;

// 테스트 결과 출력 헬퍼
void PrintTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

// 1. 기본 Push/Pop 테스트
bool TestBasicPushPop() {
    ThreadSafeQueue<int> queue(10);
    
    // Push
    assert(queue.Push(1));
    assert(queue.Push(2));
    assert(queue.Push(3));
    assert(queue.Size() == 3);
    
    // Pop
    int value;
    assert(queue.Pop(value));
    assert(value == 1);
    assert(queue.Pop(value));
    assert(value == 2);
    assert(queue.Pop(value));
    assert(value == 3);
    assert(queue.Empty());
    
    return true;
}

// 2. 큐 가득 참 테스트
bool TestQueueFull() {
    ThreadSafeQueue<int> queue(3);  // 최대 3개
    
    assert(queue.Push(1));
    assert(queue.Push(2));
    assert(queue.Push(3));
    assert(queue.IsFull());
    
    // 4번째 Push는 blocking되어야 함 -> TryPush로 테스트
    bool pushed = queue.TryPush(4, 100ms);
    assert(!pushed);  // 타임아웃으로 실패해야 함
    
    // Pop 후 다시 Push 가능
    int value;
    assert(queue.Pop(value));
    assert(queue.Push(4));
    
    return true;
}

// 3. 타임아웃 테스트
bool TestTimeout() {
    ThreadSafeQueue<int> queue(10);
    
    // TryPop on empty queue (타임아웃)
    int value;
    auto start = std::chrono::steady_clock::now();
    bool popped = queue.TryPop(value, 100ms);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    assert(!popped);
    assert(elapsed >= 100ms);
    assert(elapsed < 150ms);  // 약간의 여유
    
    return true;
}

// 4. 멀티스레드 Producer-Consumer 테스트
bool TestMultiThreaded() {
    ThreadSafeQueue<int> queue(1000);
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 1000;
    
    std::atomic<int> produced_count{0};
    std::atomic<int> consumed_count{0};
    std::atomic<int> sum_produced{0};
    std::atomic<int> sum_consumed{0};
    
    // Producers
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
                int value = i * ITEMS_PER_PRODUCER + j;
                queue.Push(value);
                produced_count++;
                sum_produced += value;
            }
        });
    }
    
    // Consumers
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back([&]() {
            while (consumed_count < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
                int value;
                if (queue.TryPop(value, 10ms)) {
                    consumed_count++;
                    sum_consumed += value;
                }
            }
        });
    }
    
    // Join
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    
    // 검증
    assert(produced_count == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    assert(consumed_count == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    assert(sum_produced == sum_consumed);
    assert(queue.Empty());
    
    return true;
}

// 5. Shutdown 테스트
bool TestShutdown() {
    ThreadSafeQueue<int> queue(10);
    
    // Producer 스레드 (blocking)
    std::atomic<bool> producer_exited{false};
    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            if (!queue.Push(i)) {
                break;  // Shutdown으로 인한 실패
            }
        }
        producer_exited = true;
    });
    
    // Consumer 스레드 (blocking)
    std::atomic<bool> consumer_exited{false};
    std::atomic<int> consumed_count{0};
    std::thread consumer([&]() {
        int value;
        while (queue.Pop(value)) {
            consumed_count++;
        }
        consumer_exited = true;
    });
    
    // 잠시 대기
    std::this_thread::sleep_for(50ms);
    
    // Shutdown 호출
    queue.Shutdown();
    
    // 스레드들이 종료되어야 함
    producer.join();
    consumer.join();
    
    assert(producer_exited);
    assert(consumer_exited);
    assert(queue.IsShutdown());
    
    return true;
}

// 6. Clear 테스트
bool TestClear() {
    ThreadSafeQueue<int> queue(10);
    
    queue.Push(1);
    queue.Push(2);
    queue.Push(3);
    assert(queue.Size() == 3);
    
    queue.Clear();
    assert(queue.Empty());
    assert(queue.Size() == 0);
    
    // Clear 후 다시 사용 가능
    queue.Push(4);
    int value;
    assert(queue.Pop(value));
    assert(value == 4);
    
    return true;
}

// 7. Move-only 타입 테스트
bool TestMoveOnly() {
    struct MoveOnlyType {
        int value;
        MoveOnlyType(int v) : value(v) {}
        MoveOnlyType(MoveOnlyType&& other) noexcept : value(other.value) {
            other.value = -1;
        }
        MoveOnlyType& operator=(MoveOnlyType&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }
        
        // 복사 방지
        MoveOnlyType(const MoveOnlyType&) = delete;
        MoveOnlyType& operator=(const MoveOnlyType&) = delete;
    };
    
    ThreadSafeQueue<MoveOnlyType> queue(10);
    
    queue.Push(MoveOnlyType(42));
    
    MoveOnlyType item(0);
    assert(queue.Pop(item));
    assert(item.value == 42);
    
    return true;
}

// 8. 성능 테스트 (참고용)
void TestPerformance() {
    ThreadSafeQueue<int> queue(10000);
    const int NUM_ITEMS = 100000;
    
    auto start = std::chrono::steady_clock::now();
    
    // Producer
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            queue.Push(i);
        }
    });
    
    // Consumer
    std::thread consumer([&]() {
        int value;
        for (int i = 0; i < NUM_ITEMS; ++i) {
            queue.Pop(value);
        }
    });
    
    producer.join();
    consumer.join();
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    std::cout << "[PERF] " << NUM_ITEMS << " items in " << ms << "ms"
              << " (" << (NUM_ITEMS * 1000.0 / ms) << " ops/sec)" << std::endl;
}

int main() {
    std::cout << "=== ThreadSafeQueue Unit Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        PrintTestResult("Basic Push/Pop", TestBasicPushPop());
        PrintTestResult("Queue Full", TestQueueFull());
        PrintTestResult("Timeout", TestTimeout());
        PrintTestResult("Multi-threaded", TestMultiThreaded());
        PrintTestResult("Shutdown", TestShutdown());
        PrintTestResult("Clear", TestClear());
        PrintTestResult("Move-only Type", TestMoveOnly());
        
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