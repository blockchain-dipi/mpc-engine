// src/common/utils/queue/ThreadSafeQueue.hpp
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <stdexcept>

namespace mpc_engine::utils
{
    // Queue 연산 결과
    enum class QueueResult 
    {
        SUCCESS = 0,      // 성공
        SHUTDOWN = 1,     // Queue가 shutdown 상태
        TIMEOUT = 2,      // 타임아웃 발생
        FULL = 3          // Queue가 가득참 (TryPush에서만)
    };

    // 결과를 문자열로 변환
    inline const char* QueueResultToString(QueueResult result) 
    {
        switch (result) {
            case QueueResult::SUCCESS: return "SUCCESS";
            case QueueResult::SHUTDOWN: return "SHUTDOWN";
            case QueueResult::TIMEOUT: return "TIMEOUT";
            case QueueResult::FULL: return "FULL";
            default: return "UNKNOWN";
        }
    }

    template<typename TElement>
    class ThreadSafeQueue 
    {
    private:
        std::queue<TElement> queue;
        mutable std::mutex mutex;
        std::condition_variable cv_not_empty;
        std::condition_variable cv_not_full;
        size_t max_size;
        std::atomic<bool> shutdown_flag{false};

    public:
        explicit ThreadSafeQueue(size_t max_size = 10000) : max_size(max_size) 
        {
            if (max_size == 0) {
                throw std::invalid_argument("Queue max_size must be greater than 0");
            }
        }

        ~ThreadSafeQueue() 
        {
            Shutdown();
        }

        // 복사 방지
        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

        // Push: Queue에 아이템 추가 (Queue가 가득 차면 대기)
        QueueResult Push(TElement item) 
        {
            std::unique_lock<std::mutex> lock(mutex);

            // Queue가 가득 찼으면 공간이 생길 때까지 대기
            cv_not_full.wait(lock, [this]() {
                return queue.size() < max_size || shutdown_flag;
            });

            if (shutdown_flag) {
                return QueueResult::SHUTDOWN;  // ✅ item은 소멸자에서 자동 정리
            }

            queue.push(std::move(item));
            cv_not_empty.notify_one();
            return QueueResult::SUCCESS;
        }

        // TryPush: 타임아웃과 함께 Push 시도
        QueueResult TryPush(TElement item, std::chrono::milliseconds timeout) 
        {
            std::unique_lock<std::mutex> lock(mutex);

            if (!cv_not_full.wait_for(lock, timeout, [this]() {
                return queue.size() < max_size || shutdown_flag;
            })) {
                return QueueResult::TIMEOUT;
            }

            if (shutdown_flag) {
                return QueueResult::SHUTDOWN;
            }

            queue.push(std::move(item));
            cv_not_empty.notify_one();
            return QueueResult::SUCCESS;
        }

        // Emplace: 큐 내부에서 직접 생성
        template<typename... Args>
        QueueResult Emplace(Args&&... args) 
        {
            std::unique_lock<std::mutex> lock(mutex);
        
            cv_not_full.wait(lock, [this]() {
                return queue.size() < max_size || shutdown_flag;
            });
        
            if (shutdown_flag) {
                return QueueResult::SHUTDOWN;
            }
        
            queue.emplace(std::forward<Args>(args)...);
            cv_not_empty.notify_one();
            return QueueResult::SUCCESS;
        }

        // TryEmplace: 타임아웃과 함께 Emplace
        template<typename... Args>
        QueueResult TryEmplace(std::chrono::milliseconds timeout, Args&&... args) 
        {
            std::unique_lock<std::mutex> lock(mutex);
        
            if (!cv_not_full.wait_for(lock, timeout, [this]() {
                return queue.size() < max_size || shutdown_flag;
            })) {
                return QueueResult::TIMEOUT;
            }
        
            if (shutdown_flag) {
                return QueueResult::SHUTDOWN;
            }
        
            queue.emplace(std::forward<Args>(args)...);
            cv_not_empty.notify_one();
            return QueueResult::SUCCESS;
        }

        // Pop: Queue에서 아이템 꺼내기 (Queue가 비어있으면 대기)
        QueueResult Pop(TElement& item) 
        {
            std::unique_lock<std::mutex> lock(mutex);

            // Queue에 아이템이 있을 때까지 대기
            cv_not_empty.wait(lock, [this]() {
                return !queue.empty() || shutdown_flag;
            });

            if (shutdown_flag && queue.empty()) {
                return QueueResult::SHUTDOWN;
            }

            item = std::move(queue.front());
            queue.pop();
            cv_not_full.notify_one();
            return QueueResult::SUCCESS;
        }

        // TryPop: 타임아웃과 함께 Pop 시도
        QueueResult TryPop(TElement& item, std::chrono::milliseconds timeout) 
        {
            std::unique_lock<std::mutex> lock(mutex);

            if (!cv_not_empty.wait_for(lock, timeout, [this]() {
                return !queue.empty() || shutdown_flag;
            })) {
                return QueueResult::TIMEOUT;
            }

            if (shutdown_flag && queue.empty()) {
                return QueueResult::SHUTDOWN;
            }

            item = std::move(queue.front());
            queue.pop();
            cv_not_full.notify_one();
            return QueueResult::SUCCESS;
        }

        // Shutdown: Queue 종료 (대기 중인 모든 스레드 깨우기)
        void Shutdown() 
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                shutdown_flag = true;
            }
            cv_not_empty.notify_all();
            cv_not_full.notify_all();
        }

        // Size: 현재 Queue 크기
        size_t Size() const 
        {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.size();
        }

        // Empty: Queue가 비어있는지 확인
        bool Empty() const 
        {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.empty();
        }

        // IsFull: Queue가 가득 찼는지 확인
        bool IsFull() const 
        {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.size() >= max_size;
        }

        // IsShutdown: 종료 상태 확인
        bool IsShutdown() const 
        {
            return shutdown_flag.load();
        }

        // Clear: Queue 비우기
        void Clear() 
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::queue<TElement> empty_queue;
            std::swap(queue, empty_queue);
            cv_not_full.notify_all();
        }

        // MaxSize: 최대 크기 반환
        size_t MaxSize() const 
        {
            return max_size;
        }

    };

} // namespace mpc_engine::utils