// src/common/network/https/include/HttpsClient.hpp
#pragma once
#include "HttpsClient.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <chrono>

namespace mpc_engine::https
{
    /**
     * @brief 연결 풀 설정
     */
    struct ConnectionPoolConfig 
    {
        uint32_t max_connections = 10;        // 최대 연결 수
        uint32_t min_idle = 2;                 // 최소 유지 연결
        uint32_t max_idle_time_ms = 60000;    // 유휴 연결 TTL (60초)
        uint32_t health_check_interval_ms = 30000;  // Health Check 주기
    };

    /**
     * @brief HTTPS Connection Pool Entry
     */
    struct PooledConnection 
    {
        std::unique_ptr<HttpsClient> client;
        std::string host;
        uint16_t port = 0;
        uint64_t last_used_time = 0;
        bool in_use = false;
        bool healthy = true;

        PooledConnection() = default;
        PooledConnection(PooledConnection&&) noexcept = default;
        PooledConnection& operator=(PooledConnection&&) noexcept = default;
    };

    /**
     * @brief HTTPS Connection Pool
     * 
     * - 연결 재사용으로 성능 향상
     * - 자동 Health Check
     * - TTL 기반 자동 정리
     */
    class HttpsConnectionPool 
    {
    private:
        ConnectionPoolConfig config_;
        std::vector<PooledConnection> connections_;
        std::mutex mutex_;
        
        const network::tls::TlsContext* tls_ctx_ = nullptr;
        bool is_initialized_ = false;

    public:
        explicit HttpsConnectionPool(const ConnectionPoolConfig& config);
        ~HttpsConnectionPool();

        /**
         * @brief 초기화 (TLS Context 설정)
         * 
         * @param tls_ctx 초기화된 TLS Context (CA 로드 완료)
         */
        bool Initialize(const network::tls::TlsContext& tls_ctx);

        /**
         * @brief 연결 획득 (없으면 생성)
         * 
         * @param host 호스트명
         * @param port 포트
         * @return HttpsClient 포인터 (사용 후 Release 필요)
         */
        HttpsClient* Acquire(const std::string& host, uint16_t port = 443);

        /**
         * @brief 연결 반환
         * 
         * @param client Acquire로 받은 클라이언트
         */
        void Release(HttpsClient* client);

        /**
         * @brief 모든 연결 종료
         */
        void CloseAll();

        /**
         * @brief 유휴 연결 정리
         */
        void CleanupIdleConnections();

        /**
         * @brief 통계 정보
         */
        struct Stats {
            uint32_t total = 0;
            uint32_t in_use = 0;
            uint32_t idle = 0;
            uint32_t healthy = 0;
        };
        Stats GetStats();

    private:
        HttpsClient* FindOrCreateConnection(const std::string& host, uint16_t port);
        void RemoveUnhealthyConnections();
        uint64_t GetCurrentTimeMs() const;
    };

} // namespace mpc_engine::https