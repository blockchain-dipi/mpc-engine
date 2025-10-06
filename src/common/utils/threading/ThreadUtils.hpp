// src/common/utils/threading/ThreadUtils.hpp
#pragma once
#include <thread>
#include <chrono>
#include <future>
#include <vector>

namespace mpc_engine::utils
{
    enum class JoinResult 
    {
        SUCCESS = 0,
        TIMEOUT = 1,
        NOT_JOINABLE = 2
    };

    inline const char* ToString(JoinResult result) 
    {
        switch (result) {
            case JoinResult::SUCCESS: return "SUCCESS";
            case JoinResult::TIMEOUT: return "TIMEOUT";
            case JoinResult::NOT_JOINABLE: return "NOT_JOINABLE";
            default: return "UNKNOWN";
        }
    }

    /**
     * @brief Thread를 타임아웃과 함께 join
     * 
     * @param thread Join할 스레드
     * @param timeout_ms 타임아웃 (밀리초)
     * @return JoinResult 성공/타임아웃/불가
     */
    inline JoinResult JoinWithTimeout(std::thread& thread, uint32_t timeout_ms) 
    {
        if (!thread.joinable()) {
            return JoinResult::NOT_JOINABLE;
        }

        // std::thread는 timed_join이 없으므로 future 사용
        auto future = std::async(std::launch::async, [&thread]() {
            thread.join();
        });

        auto status = future.wait_for(std::chrono::milliseconds(timeout_ms));
        
        if (status == std::future_status::ready) {
            return JoinResult::SUCCESS;
        } else {
            // 타임아웃 발생 - 강제 detach (위험하지만 무한 대기보다 낫다)
            thread.detach();
            return JoinResult::TIMEOUT;
        }
    }

    /**
     * @brief 여러 스레드를 타임아웃과 함께 join
     * 
     * @param threads Join할 스레드들
     * @param timeout_ms 개별 스레드당 타임아웃
     * @return 성공한 스레드 수
     */
    inline size_t JoinAllWithTimeout(std::vector<std::thread*> threads, uint32_t timeout_ms) 
    {
        size_t success_count = 0;
        
        for (auto* thread : threads) {
            if (!thread) continue;
            
            JoinResult result = JoinWithTimeout(*thread, timeout_ms);
            if (result == JoinResult::SUCCESS) {
                success_count++;
            }
        }
        
        return success_count;
    }

    /**
     * @brief 안전한 스레드 종료 헬퍼
     * 
     * 1. stop_flag를 true로 설정
     * 2. wake_condition을 notify (optional)
     * 3. 타임아웃과 함께 join
     */
    template<typename TStopFlag, typename TWakeCondition = void*>
    inline JoinResult SafeStopThread(
        std::thread& thread,
        TStopFlag& stop_flag,
        uint32_t timeout_ms,
        TWakeCondition* wake_condition = nullptr
    ) {
        // 1. 종료 플래그 설정
        stop_flag.store(true);
        
        // 2. 대기 중인 스레드 깨우기 (optional)
        if constexpr (!std::is_same_v<TWakeCondition, void*>) {
            if (wake_condition) {
                wake_condition->notify_all();
            }
        }
        
        // 3. 타임아웃과 함께 join
        return JoinWithTimeout(thread, timeout_ms);
    }
}