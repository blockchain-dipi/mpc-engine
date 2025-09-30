// src/common/utils/threading/ThreadPool.hpp
#pragma once

#include "common/utils/queue/ThreadSafeQueue.hpp"
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <memory>

namespace mpc_engine::utils
{
    template<typename ContextType>
    class ThreadPool 
    {
    private:
        // 타입 안전한 Task
        struct Task 
        {
            void (*func)(ContextType*);
            ContextType* context;
            
            void operator()() const 
            {
                func(context);
            }
        };
        
        std::vector<std::thread> workers;
        ThreadSafeQueue<Task> task_queue;
        std::atomic<bool> stop{false};
        std::atomic<size_t> active_tasks{0};
        size_t num_threads;

    public:
        explicit ThreadPool(size_t num_threads) 
        : task_queue(num_threads * 100), num_threads(num_threads)
        {
            if (num_threads == 0) {
                throw std::invalid_argument("ThreadPool must have at least 1 thread");
            }

            workers.reserve(num_threads);
            for (size_t i = 0; i < num_threads; ++i) {
                workers.emplace_back([this, i]() { 
                    WorkerLoop(i); 
                });
            }
        }

        ~ThreadPool() 
        {
            Shutdown();
        }

        // 복사/이동 방지
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        // 작업 제출
        void Submit(void (*func)(ContextType*), ContextType* context) 
        {
            if (stop) {
                throw std::runtime_error("ThreadPool is stopped");
            }

            // 🆕 QueueResult 확인
            QueueResult result = task_queue.Push(Task{func, context});
            
            if (result == QueueResult::SHUTDOWN) {
                throw std::runtime_error("ThreadPool queue is shutdown");
            } else if (result != QueueResult::SUCCESS) {
                throw std::runtime_error("Failed to push task: " + std::string(ToString(result)));
            }
        }

        // Graceful shutdown
        void Shutdown() 
        {
            if (stop.exchange(true)) {
                return;  // 이미 종료 중
            }
            
            // Queue 종료 (대기 중인 워커들 깨우기)
            task_queue.Shutdown();
            
            // 모든 워커 종료 대기
            for (auto& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

        // 현재 활성 작업 수
        size_t GetActiveTaskCount() const 
        {
            return active_tasks.load();
        }

        // 대기 중인 작업 수
        size_t GetPendingTaskCount() const 
        {
            return task_queue.Size();
        }

        // 워커 스레드 수
        size_t GetThreadCount() const 
        {
            return num_threads;
        }

        // 종료 상태 확인
        bool IsStopped() const 
        {
            return stop.load();
        }

    private:
        void WorkerLoop(size_t worker_id) 
        {
            while (!stop) {
                Task task;

                // 🆕 QueueResult 사용
                QueueResult result = task_queue.Pop(task);
                
                if (result == QueueResult::SHUTDOWN) {
                    // Shutdown 시그널
                    break;
                }
                
                if (result != QueueResult::SUCCESS) {
                    // 예상치 못한 에러
                    fprintf(stderr, "[ThreadPool Worker %zu] Unexpected pop result: %s\n", 
                            worker_id, ToString(result));
                    break;
                }

                active_tasks++;
                
                try {
                    // 직접 함수 포인터 호출
                    task.func(task.context);
                } catch (const std::exception& e) {
                    // 예외 로깅
                    fprintf(stderr, "[ThreadPool Worker %zu] Exception: %s\n", 
                            worker_id, e.what());
                } catch (...) {
                    fprintf(stderr, "[ThreadPool Worker %zu] Unknown exception\n", 
                            worker_id);
                }
                
                active_tasks--;
            }
        }
    };

} // namespace mpc_engine::utils