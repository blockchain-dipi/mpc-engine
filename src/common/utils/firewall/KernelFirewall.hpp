// src/common/utils/firewall/KernelFirewall.hpp
#pragma once
#include <string>
#include <cstdint>

namespace mpc_engine::utils
{
    class KernelFirewall 
    {
    public:
        // 방화벽 규칙 설정
        // - SYN 패킷 레벨에서 차단 (커널이 소켓 생성 전에 DROP)
        // - 신뢰할 IP만 허용
        static bool ConfigureNodeFirewall(
            uint16_t port,
            const std::string& trusted_coordinator_ip,
            bool dry_run = false
        );

        // 방화벽 규칙 제거
        static bool RemoveNodeFirewall(
            uint16_t port,
            bool dry_run = false
        );

        // 현재 규칙 확인
        static bool IsFirewallConfigured(uint16_t port);

        // 루트 권한 확인
        static bool HasRootPrivilege();

    private:
        // iptables 명령 실행
        static bool ExecuteIptables(const std::string& command, bool dry_run);
        
        // 규칙 존재 확인
        static bool RuleExists(const std::string& rule_pattern);
        
        // 안전한 IP 주소 검증
        static bool IsValidIPv4(const std::string& ip);
    };
} // namespace mpc_engine::utils