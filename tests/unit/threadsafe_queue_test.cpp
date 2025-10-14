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

void PrintTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

// Test 1: 기본 Push/Pop (QueueResult 사용)
bool TestBasicPushPop() {
    ThreadSafeQueue<int> queue(10);
    
    // Push
    assert(queue.Push(1) == QueueResult::SUCCESS);
    assert(queue.Push(2) == QueueResult::SUCCESS);
    assert(queue.Push(3) == QueueResult::SUCCESS);
    assert(queue.Size() == 3);
    
    // Pop
    int value;
    assert(queue.Pop(value) == QueueResult::SUCCESS);
    assert(value == 1);
    assert(queue.Pop(value) == QueueResult::SUCCESS);
    assert(value == 2);
    assert(queue.Pop(value) == QueueResult::SUCCESS);
    assert(value == 3);
    assert(queue.Empty());
    
    return true;
}

// Test 2: Queue 가득 참
bool TestQueueFull() {
    ThreadSafeQueue<int> queue(3);
    
    assert(queue.Push(1) == QueueResult::SUCCESS);
    assert(queue.Push(2) == QueueResult::SUCCESS);
    assert(queue.Push(3) == QueueResult::SUCCESS);
    assert(queue.IsFull());
    
    // TryPush로 타임아웃 테스트
    QueueResult result = queue.TryPush(4, 100ms);
    assert(result == QueueResult::TIMEOUT);
    
    // Pop 후 다시 Push 가능
    int value;
    assert(queue.Pop(value) == QueueResult::SUCCESS);
    assert(queue.Push(4) == QueueResult::SUCCESS);
    
    return true;
}

// Test 3: 타임아웃 테스트
bool TestTimeout() {
    ThreadSafeQueue<int> queue(10);
    
    // TryPop on empty queue
    int value;
    auto start = std::chrono::steady_clock::now();
    QueueResult result = queue.TryPop(value, 100ms);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    assert(result == QueueResult::TIMEOUT);
    assert(elapsed >= 100ms);
    assert(elapsed < 150ms);
    
    return true;
}

// Test 4: Shutdown 테스트
bool TestShutdown() {
    ThreadSafeQueue<int> queue(10);
    
    std::atomic<QueueResult> producer_result{QueueResult::SUCCESS};
    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            QueueResult result = queue.Push(i);
            if (result != QueueResult::SUCCESS) {
                producer_result = result;
                break;
            }
        }
    });
    
    std::atomic<QueueResult> consumer_result{QueueResult::SUCCESS};
    std::atomic<int> consumed_count{0};
    std::thread consumer([&]() {
        int value;
        while (true) {
            QueueResult result = queue.Pop(value);
            if (result == QueueResult::SHUTDOWN) {
                consumer_result = result;
                break;
            }
            if (result == QueueResult::SUCCESS) {
                consumed_count++;
            }
        }
    });
    
    std::this_thread::sleep_for(50ms);
    
    queue.Shutdown();
    
    producer.join();
    consumer.join();
    
    assert(producer_result == QueueResult::SHUTDOWN || consumed_count > 0);
    assert(consumer_result == QueueResult::SHUTDOWN);
    assert(queue.IsShutdown());
    
    std::cout << "  Producer result: " << QueueResultToString(producer_result.load()) << std::endl;
    std::cout << "  Consumer result: " << QueueResultToString(consumer_result.load()) << std::endl;
    std::cout << "  Consumed: " << consumed_count << " items" << std::endl;
    
    return true;
}

// Test 5: ToString 테스트
bool TestToString() {
    assert(std::string(QueueResultToString(QueueResult::SUCCESS)) == "SUCCESS");
    assert(std::string(QueueResultToString(QueueResult::SHUTDOWN)) == "SHUTDOWN");
    assert(std::string(QueueResultToString(QueueResult::TIMEOUT)) == "TIMEOUT");
    assert(std::string(QueueResultToString(QueueResult::FULL)) == "FULL");
    
    return true;
}

// Test 6: 멀티스레드
bool TestMultiThreaded() {
    ThreadSafeQueue<int> queue(1000);
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 1000;
    
    std::atomic<int> produced_count{0};
    std::atomic<int> consumed_count{0};
    std::atomic<int> sum_produced{0};
    std::atomic<int> sum_consumed{0};
    
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
                int value = i * ITEMS_PER_PRODUCER + j;
                QueueResult result = queue.Push(value);
                if (result == QueueResult::SUCCESS) {
                    produced_count++;
                    sum_produced += value;
                }
            }
        });
    }
    
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back([&]() {
            while (consumed_count < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
                int value;
                QueueResult result = queue.TryPop(value, 10ms);
                if (result == QueueResult::SUCCESS) {
                    consumed_count++;
                    sum_consumed += value;
                }
            }
        });
    }
    
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    
    assert(produced_count == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    assert(consumed_count == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    assert(sum_produced == sum_consumed);
    assert(queue.Empty());
    
    return true;
}

// Test 7: Clear
bool TestClear() {
    ThreadSafeQueue<int> queue(10);
    
    queue.Push(1);
    queue.Push(2);
    queue.Push(3);
    assert(queue.Size() == 3);
    
    queue.Clear();
    assert(queue.Empty());
    assert(queue.Size() == 0);
    
    assert(queue.Push(4) == QueueResult::SUCCESS);
    int value;
    assert(queue.Pop(value) == QueueResult::SUCCESS);
    assert(value == 4);
    
    return true;
}

// Test 8: Shutdown 후 Push/Pop
bool TestShutdownPushPop() {
    ThreadSafeQueue<int> queue(10);
    
    queue.Push(1);
    queue.Push(2);
    
    queue.Shutdown();
    
    QueueResult push_result = queue.Push(3);
    assert(push_result == QueueResult::SHUTDOWN);
    
    int value;
    QueueResult pop_result1 = queue.Pop(value);
    assert(pop_result1 == QueueResult::SUCCESS);
    assert(value == 1);
    
    pop_result1 = queue.Pop(value);
    assert(pop_result1 == QueueResult::SUCCESS);
    assert(value == 2);
    
    QueueResult pop_result2 = queue.Pop(value);
    assert(pop_result2 == QueueResult::SHUTDOWN);
    
    std::cout << "  Push after shutdown: " << QueueResultToString(push_result) << std::endl;
    std::cout << "  Pop after empty: " << QueueResultToString(pop_result2) << std::endl;
    
    return true;
}

void TestPerformance() {
    ThreadSafeQueue<int> queue(10000);
    const int NUM_ITEMS = 100000;
    
    auto start = std::chrono::steady_clock::now();
    
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            queue.Push(i);
        }
    });
    
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
    std::cout << "=== ThreadSafeQueue Tests (QueueResult) ===" << std::endl;
    std::cout << std::endl;
    
    try {
        PrintTestResult("Basic Push/Pop", TestBasicPushPop());
        PrintTestResult("Queue Full", TestQueueFull());
        PrintTestResult("Timeout", TestTimeout());
        PrintTestResult("Shutdown", TestShutdown());
        PrintTestResult("ToString", TestToString());
        PrintTestResult("Multi-threaded", TestMultiThreaded());
        PrintTestResult("Clear", TestClear());
        PrintTestResult("Shutdown Push/Pop", TestShutdownPushPop());
        
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