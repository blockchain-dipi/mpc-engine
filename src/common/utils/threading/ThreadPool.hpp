// src/common/utils/threading/ThreadPool.hpp
#pragma once

#include "common/utils/queue/ThreadSafeQueue.hpp"
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <future>
#include <memory>

namespace mpc_engine::utils
{
    class ThreadPool 
    {
    private:
        std::vector<std::thread> workers;
        ThreadSafeQueue<std::function<void()>> task_queue;
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

        // 작업 제출 (반환값 없음)
        template<typename F>
        void Submit(F&& task) 
        {
            if (stop) {
                throw std::runtime_error("ThreadPool is stopped");
            }

            task_queue.Push(std::forward<F>(task));
        }

        // 작업 제출 (반환값 있음)
        template<typename F, typename... Args>
        auto SubmitTask(F&& func, Args&&... args) 
            -> std::future<typename std::invoke_result<F, Args...>::type>
        {
            using return_type = typename std::invoke_result<F, Args...>::type;

            if (stop) {
                throw std::runtime_error("ThreadPool is stopped");
            }

            // packaged_task로 래핑
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(func), std::forward<Args>(args)...)
            );

            std::future<return_type> result = task->get_future();

            // 큐에 추가
            task_queue.Push([task]() { 
                (*task)(); 
            });

            return result;
        }

        // Graceful shutdown
        void Shutdown() 
        {
            if (stop.exchange(true)) {
                return;  // 이미 종료 중
            }

            // 큐 종료 (대기 중인 워커들 깨우기)
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
                std::function<void()> task;

                // 큐에서 작업 가져오기 (blocking)
                if (task_queue.Pop(task)) {
                    active_tasks++;
                    
                    try {
                        task();
                    } catch (const std::exception& e) {
                        // 예외 로깅 (실제로는 Logger 사용)
                        fprintf(stderr, "[ThreadPool Worker %zu] Exception: %s\n", worker_id, e.what());
                    } catch (...) {
                        fprintf(stderr, "[ThreadPool Worker %zu] Unknown exception\n", worker_id);
                    }
                    
                    active_tasks--;
                } else {
                    // Shutdown 시그널
                    break;
                }
            }
        }
    };

} // namespace mpc_engine::utils