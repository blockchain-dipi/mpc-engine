// src/coordinator/network/wallet_server/src/WalletConnectionInfo.cpp
#include "coordinator/network/wallet_server/include/WalletConnectionInfo.hpp"
#include <regex>
#include <iostream>

namespace mpc_engine::coordinator::network
{
    bool WalletConnectionInfo::ParseUrl(const std::string& url) 
    {
        std::regex url_pattern(
            R"(^https?://([^:/]+)(?::(\d+))?(/.*)?$)",
            std::regex::icase
        );
        
        std::smatch matches;
        if (!std::regex_match(url, matches, url_pattern)) {
            std::cerr << "[WalletConnectionInfo] Invalid URL: " << url << std::endl;
            return false;
        }
        
        host = matches[1].str();
        port = matches[2].matched ? static_cast<uint16_t>(std::stoi(matches[2].str())) : 443;
        path_prefix = matches[3].matched ? matches[3].str() : "";
        
        if (!path_prefix.empty() && path_prefix.back() == '/') {
            path_prefix.pop_back();
        }
        
        wallet_url = url;
        return true;
    }

} // namespace