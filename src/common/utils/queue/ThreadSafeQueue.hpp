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
    template<typename T>
    class ThreadSafeQueue 
    {
    private:
        std::queue<T> queue;
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

        // Push: 큐에 아이템 추가 (큐가 가득 차면 대기)
        bool Push(T item) 
        {
            std::unique_lock<std::mutex> lock(mutex);

            // 큐가 가득 찼으면 공간이 생길 때까지 대기
            cv_not_full.wait(lock, [this]() {
                return queue.size() < max_size || shutdown_flag;
            });

            if (shutdown_flag) {
                return false;
            }

            queue.push(std::move(item));
            cv_not_empty.notify_one();
            return true;
        }

        // TryPush: 타임아웃과 함께 Push 시도
        bool TryPush(T item, std::chrono::milliseconds timeout) 
        {
            std::unique_lock<std::mutex> lock(mutex);

            if (!cv_not_full.wait_for(lock, timeout, [this]() {
                return queue.size() < max_size || shutdown_flag;
            })) {
                return false;  // 타임아웃
            }

            if (shutdown_flag) {
                return false;
            }

            queue.push(std::move(item));
            cv_not_empty.notify_one();
            return true;
        }

        // Pop: 큐에서 아이템 꺼내기 (큐가 비어있으면 대기)
        bool Pop(T& item) 
        {
            std::unique_lock<std::mutex> lock(mutex);

            // 큐에 아이템이 있을 때까지 대기
            cv_not_empty.wait(lock, [this]() {
                return !queue.empty() || shutdown_flag;
            });

            if (shutdown_flag && queue.empty()) {
                return false;
            }

            item = std::move(queue.front());
            queue.pop();
            cv_not_full.notify_one();
            return true;
        }

        // TryPop: 타임아웃과 함께 Pop 시도
        bool TryPop(T& item, std::chrono::milliseconds timeout) 
        {
            std::unique_lock<std::mutex> lock(mutex);

            if (!cv_not_empty.wait_for(lock, timeout, [this]() {
                return !queue.empty() || shutdown_flag;
            })) {
                return false;  // 타임아웃
            }

            if (shutdown_flag && queue.empty()) {
                return false;
            }

            item = std::move(queue.front());
            queue.pop();
            cv_not_full.notify_one();
            return true;
        }

        // Shutdown: 큐 종료 (대기 중인 모든 스레드 깨우기)
        void Shutdown() 
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                shutdown_flag = true;
            }
            cv_not_empty.notify_all();
            cv_not_full.notify_all();
        }

        // Size: 현재 큐 크기
        size_t Size() const 
        {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.size();
        }

        // Empty: 큐가 비어있는지 확인
        bool Empty() const 
        {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.empty();
        }

        // IsFull: 큐가 가득 찼는지 확인
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

        // Clear: 큐 비우기
        void Clear() 
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::queue<T> empty_queue;
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