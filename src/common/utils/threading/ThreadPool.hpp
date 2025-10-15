// src/common/utils/threading/ThreadPool.hpp
#pragma once

#include "common/utils/queue/ThreadSafeQueue.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

namespace mpc_engine::utils
{
    template<typename TContext>
    class ThreadPool 
    {
    private:
        struct Task 
        {
            void (*func)(TContext*);
            TContext* context;
            bool owned;  // 소유권 플래그
            
            Task() : func(nullptr), context(nullptr), owned(false) {}

            Task(void (*f)(TContext*), TContext* c, bool o) 
                : func(f), context(c), owned(o) {}

            Task(Task&& other) noexcept : func(other.func), context(other.context), owned(other.owned)
            {
                other.context = nullptr;
                other.owned = false;
            }

            Task& operator=(Task&& other) noexcept
            {
                if (this != &other) {
                    if (owned && context) {
                        delete context;
                    }
                    func = other.func;
                    context = other.context;
                    owned = other.owned;
                    other.context = nullptr;
                    other.owned = false;
                }
                return *this;
            }

            // 복사 금지
            Task(const Task&) = delete;
            Task& operator=(const Task&) = delete;

            ~Task() {
                if (owned && context) {
                    delete context;
                    context = nullptr;
                }
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

        /**
         * @brief 작업 제출 (자동 삭제)
         * 
         * ThreadPool이 context를 소유하고 작업 완료 후 자동 삭제합니다.
         * 
         * @param func 작업 함수
         * @param context unique_ptr (소유권 이전)
         */
        void SubmitOwned(void (*func)(TContext*), std::unique_ptr<TContext> context) 
        {
            TContext* raw_ptr = context.release();
            
            if (stop) {
                delete raw_ptr;
                throw std::runtime_error("ThreadPool is stopped");
            }
        
            QueueResult result = task_queue.Emplace(func, raw_ptr, true);
            if (result != QueueResult::SUCCESS) {
                delete raw_ptr;
                throw std::runtime_error("Failed to push task");
            }
        }

        /**
         * @brief 작업 제출 (빌려줌)
         * 
         * ThreadPool은 context를 삭제하지 않습니다.
         * 
         * @param func 작업 함수
         * @param context raw pointer (호출자가 생명주기 관리)
         */
        void SubmitBorrowed(void (*func)(TContext*), TContext* context) 
        {
            if (stop) {
                throw std::runtime_error("ThreadPool is stopped");
            }

            QueueResult result = task_queue.Emplace(func, context, false);
            if (result != QueueResult::SUCCESS) {
                throw std::runtime_error("Failed to push task");
            }
        }

        void Shutdown() 
        {
            if (stop.exchange(true)) {
                return;
            }
            
            task_queue.Shutdown();
            
            for (auto& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

        size_t GetActiveTaskCount() const { return active_tasks.load(); }
        size_t GetPendingTaskCount() const { return task_queue.Size(); }
        size_t GetThreadCount() const { return num_threads; }
        bool IsStopped() const { return stop.load(); }

    private:
        void WorkerLoop(size_t worker_id) 
        {
            while (!stop) {
                Task task;

                QueueResult result = task_queue.Pop(task);
                
                if (result == QueueResult::SHUTDOWN) {
                    break;
                }
                
                if (result != QueueResult::SUCCESS) {
                    fprintf(stderr, "[ThreadPool Worker %zu] Unexpected pop result: %s\n", worker_id, QueueResultToString(result));
                    break;
                }

                active_tasks++;
                
                try {
                    task.func(task.context);
                } catch (const std::exception& e) {
                    fprintf(stderr, "[ThreadPool Worker %zu] Exception: %s\n", 
                            worker_id, e.what());
                } catch (...) {
                    fprintf(stderr, "[ThreadPool Worker %zu] Unknown exception\n", 
                            worker_id);
                }
                
                active_tasks--;
                
                // ✅ task가 스코프 벗어나며 Task::~Task() 호출
                //    owned == true면 자동으로 context 삭제
            }
        }
    };

} // namespace mpc_engine::utils