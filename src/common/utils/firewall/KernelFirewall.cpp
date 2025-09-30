// src/common/utils/firewall/KernelFirewall.cpp
#include "common/utils/firewall/KernelFirewall.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <array>
#include <memory>

namespace mpc_engine::utils
{
    bool KernelFirewall::HasRootPrivilege() 
    {
        return geteuid() == 0;
    }

    bool KernelFirewall::IsValidIPv4(const std::string& ip) 
    {
        struct sockaddr_in sa;
        return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1;
    }

    bool KernelFirewall::ExecuteIptables(const std::string& command, bool dry_run) 
    {
        if (dry_run) {
            std::cout << "[FIREWALL][DRY-RUN] " << command << std::endl;
            return true;
        }

        std::cout << "[FIREWALL] Executing: " << command << std::endl;
        
        // popen으로 명령 실행 및 출력 캡처
        std::array<char, 128> buffer;
        std::string result;
        
        std::unique_ptr<FILE, decltype(&pclose)> pipe(
            popen((command + " 2>&1").c_str(), "r"), 
            pclose
        );
        
        if (!pipe) {
            std::cerr << "[FIREWALL] Failed to execute command" << std::endl;
            return false;
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        int exit_code = pclose(pipe.release());
        
        if (exit_code != 0) {
            std::cerr << "[FIREWALL] Command failed (exit code: " << exit_code << ")" << std::endl;
            if (!result.empty()) {
                std::cerr << "[FIREWALL] Output: " << result << std::endl;
            }
            return false;
        }

        if (!result.empty()) {
            std::cout << "[FIREWALL] " << result << std::endl;
        }

        return true;
    }

    bool KernelFirewall::RuleExists(const std::string& rule_pattern) 
    {
        std::string cmd = "iptables -L INPUT -n | grep \"" + rule_pattern + "\"";
        int result = system((cmd + " > /dev/null 2>&1").c_str());
        return result == 0;
    }

    bool KernelFirewall::ConfigureNodeFirewall(
        uint16_t port,
        const std::string& trusted_coordinator_ip,
        bool dry_run
    ) 
    {
        // 1. 권한 확인
        if (!dry_run && !HasRootPrivilege()) {
            std::cerr << "[FIREWALL] Root privilege required" << std::endl;
            return false;
        }

        // 2. IP 주소 검증
        if (!IsValidIPv4(trusted_coordinator_ip)) {
            std::cerr << "[FIREWALL] Invalid IPv4 address: " << trusted_coordinator_ip << std::endl;
            return false;
        }

        std::cout << "[FIREWALL] Configuring kernel-level firewall" << std::endl;
        std::cout << "[FIREWALL]   Port: " << port << std::endl;
        std::cout << "[FIREWALL]   Trusted IP: " << trusted_coordinator_ip << std::endl;

        // 3. 기존 규칙 제거 (멱등성 보장)
        RemoveNodeFirewall(port, dry_run);

        // 4. 신뢰할 IP에서의 SYN 패킷 허용
        std::stringstream ss;
        ss << "iptables -I INPUT 1 -p tcp --syn --dport " << port 
           << " -s " << trusted_coordinator_ip << " -j ACCEPT";
        
        if (!ExecuteIptables(ss.str(), dry_run)) {
            std::cerr << "[FIREWALL] Failed to add ACCEPT rule" << std::endl;
            return false;
        }

        // 5. 나머지 모든 SYN 패킷 DROP
        ss.str("");
        ss << "iptables -A INPUT -p tcp --syn --dport " << port << " -j DROP";
        
        if (!ExecuteIptables(ss.str(), dry_run)) {
            std::cerr << "[FIREWALL] Failed to add DROP rule" << std::endl;
            RemoveNodeFirewall(port, dry_run);
            return false;
        }

        std::cout << "[FIREWALL] ✓ Kernel firewall configured successfully" << std::endl;
        std::cout << "[FIREWALL] ✓ Only " << trusted_coordinator_ip 
                  << " can establish TCP connections to port " << port << std::endl;
        
        return true;
    }

    bool KernelFirewall::RemoveNodeFirewall(uint16_t port, bool dry_run) 
    {
        if (!dry_run && !HasRootPrivilege()) {
            std::cerr << "[FIREWALL] Root privilege required" << std::endl;
            return false;
        }

        std::cout << "[FIREWALL] Removing existing firewall rules for port " << port << std::endl;

        std::stringstream ss;
        ss << "iptables -D INPUT -p tcp --syn --dport " << port << " -j DROP 2>/dev/null";
        
        for (int i = 0; i < 10; ++i) {
            if (!ExecuteIptables(ss.str(), dry_run)) {
                break;
            }
        }

        ss.str("");
        ss << "iptables -D INPUT -p tcp --syn --dport " << port << " -j ACCEPT 2>/dev/null";
        
        for (int i = 0; i < 10; ++i) {
            if (!ExecuteIptables(ss.str(), dry_run)) {
                break;
            }
        }

        std::cout << "[FIREWALL] ✓ Firewall rules removed" << std::endl;
        return true;
    }

    bool KernelFirewall::IsFirewallConfigured(uint16_t port) 
    {
        std::stringstream ss;
        ss << "dpt:" << port;
        return RuleExists(ss.str());
    }

} // namespace mpc_engine::utils