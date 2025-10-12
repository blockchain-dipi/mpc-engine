// src/common/network/https/src/HttpsConnectionPool.cpp
#include "common/network/https/include/HttpsConnectionPool.hpp"
#include <algorithm>
#include <iostream>

namespace mpc_engine::https
{
    HttpsConnectionPool::HttpsConnectionPool(const ConnectionPoolConfig& config)
        : config_(config)
    {
    }

    HttpsConnectionPool::~HttpsConnectionPool() 
    {
        CloseAll();
    }

    bool HttpsConnectionPool::Initialize(const network::tls::TlsContext& tls_ctx) 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_initialized_) {
            return true;
        }

        tls_ctx_ = &tls_ctx;
        is_initialized_ = true;

        return true;
    }

    HttpsClient* HttpsConnectionPool::Acquire(const std::string& host, uint16_t port) 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_initialized_ || !tls_ctx_) {
            return nullptr;
        }

        return FindOrCreateConnection(host, port);
    }

    void HttpsConnectionPool::Release(HttpsClient* client) 
    {
        if (!client) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 연결 찾기
        for (auto& conn : connections_) {
            if (conn.client.get() == client) {
                conn.in_use = false;
                conn.last_used_time = GetCurrentTimeMs();
                return;
            }
        }
    }

    void HttpsConnectionPool::CloseAll() 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& conn : connections_) {
            if (conn.client) {
                conn.client->Disconnect();
            }
        }

        connections_.clear();
    }

    void HttpsConnectionPool::CleanupIdleConnections() 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        uint64_t now = GetCurrentTimeMs();
        uint32_t idle_count = 0;

        // 유휴 연결 개수 세기
        for (const auto& conn : connections_) {
            if (!conn.in_use) {
                ++idle_count;
            }
        }

        // min_idle 이상 유지하면서 오래된 연결 제거
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(),
                [&](PooledConnection& conn) {
                    if (conn.in_use) {
                        return false;  // 사용 중인 연결은 유지
                    }

                    // 최소 유휴 연결 수 유지
                    if (idle_count <= config_.min_idle) {
                        return false;
                    }

                    // TTL 초과한 연결 제거
                    if (now - conn.last_used_time > config_.max_idle_time_ms) {
                        if (conn.client) {
                            conn.client->Disconnect();
                        }
                        --idle_count;
                        return true;
                    }

                    return false;
                }),
            connections_.end()
        );

        RemoveUnhealthyConnections();
    }

    HttpsConnectionPool::Stats HttpsConnectionPool::GetStats()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Stats stats;
        stats.total = connections_.size();

        for (const auto& conn : connections_) {
            if (conn.in_use) {
                ++stats.in_use;
            } else {
                ++stats.idle;
            }

            if (conn.healthy) {
                ++stats.healthy;
            }
        }

        return stats;
    }

    HttpsClient* HttpsConnectionPool::FindOrCreateConnection(
        const std::string& host, 
        uint16_t port
    ) {
        // 1. 기존 유휴 연결 찾기
        for (auto& conn : connections_) {
            if (!conn.in_use && 
                conn.host == host && 
                conn.port == port &&
                conn.healthy &&
                conn.client &&
                conn.client->IsConnected()) {
                
                conn.in_use = true;
                conn.last_used_time = GetCurrentTimeMs();
                return conn.client.get();
            }
        }

        // 2. 최대 연결 수 확인
        if (connections_.size() >= config_.max_connections) {
            std::cerr << "Connection pool full (" << config_.max_connections << ")" << std::endl;
            return nullptr;
        }

        // 3. 새 연결 생성
        PooledConnection new_conn;
        new_conn.host = host;
        new_conn.port = port;
        new_conn.in_use = true;
        new_conn.last_used_time = GetCurrentTimeMs();
        new_conn.healthy = true;

        HttpsClientConfig client_config;
        client_config.connect_timeout_ms = 5000;
        client_config.read_timeout_ms = 30000;
        client_config.write_timeout_ms = 30000;

        new_conn.client = std::make_unique<HttpsClient>(client_config);

        // 연결 시도
        if (!new_conn.client->Connect(*tls_ctx_, host, port)) {
            std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
            new_conn.healthy = false;
            return nullptr;
        }

        connections_.push_back(std::move(new_conn));
        return connections_.back().client.get();
    }

    void HttpsConnectionPool::RemoveUnhealthyConnections() 
    {
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(),
                [](PooledConnection& conn) {
                    if (!conn.healthy && !conn.in_use) {
                        if (conn.client) {
                            conn.client->Disconnect();
                        }
                        return true;
                    }
                    return false;
                }),
            connections_.end()
        );
    }

    uint64_t HttpsConnectionPool::GetCurrentTimeMs() const 
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

} // namespace mpc_engine::https