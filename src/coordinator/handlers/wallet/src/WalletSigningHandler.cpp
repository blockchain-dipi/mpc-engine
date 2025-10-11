// src/coordinator/handlers/wallet/src/WalletSigningHandler.cpp
#include "coordinator/handlers/wallet/include/WalletSigningHandler.hpp"
#include "protocols/coordinator_wallet/include/WalletSigningProtocol.hpp"
#include "common/utils/socket/SocketUtils.hpp"
#include <iostream>
#include <sstream>

namespace mpc_engine::coordinator::handlers::wallet
{
    using namespace protocol::coordinator_wallet;

    std::unique_ptr<WalletBaseResponse> HandleWalletSigningRequest(const WalletBaseRequest* request) 
    {
        std::cout << "=== HandleWalletSigningRequest ===" << std::endl;

        const WalletSigningRequest* signingReq = 
            static_cast<const WalletSigningRequest*>(request);
        
        auto response = std::make_unique<WalletSigningResponse>();
        
        // Request 정보 복사
        response->requestId = signingReq->requestId;
        response->keyId = signingReq->keyId;

        try 
        {
            std::cout << "[Handler] Processing signing request:" << std::endl;
            std::cout << "  Key ID: " << signingReq->keyId << std::endl;
            std::cout << "  Transaction: " << signingReq->transactionData.substr(0, 50) 
                      << "..." << std::endl;
            std::cout << "  Threshold: " << signingReq->threshold 
                      << "/" << signingReq->totalShards << std::endl;

            // === Mock MPC 서명 시뮬레이션 ===
            
            // 1. Shard 서명 생성 (Mock)
            for (uint32_t i = 0; i < signingReq->totalShards; ++i) {
                std::ostringstream oss;
                oss << "0xMOCK_SHARD_" << i << "_" 
                    << signingReq->keyId << "_" 
                    << utils::GetCurrentTimeMs();
                response->shardSignatures.push_back(oss.str());
            }

            // 2. 최종 서명 생성 (Mock)
            std::ostringstream final_sig;
            final_sig << "0xMOCK_FINAL_SIG_" 
                      << signingReq->keyId << "_" 
                      << utils::GetCurrentTimeMs();
            response->finalSignature = final_sig.str();

            // 3. 성공 정보
            response->successfulShards = signingReq->totalShards;
            response->success = true;
            response->timestamp = std::to_string(utils::GetCurrentTimeMs());
            
            std::cout << "[Handler] Mock signing completed successfully" << std::endl;
            std::cout << "  Final Signature: " << response->finalSignature << std::endl;
        }
        catch (const std::exception& e) 
        {
            response->success = false;
            response->errorMessage = "Wallet signing failed: " + std::string(e.what());
            std::cerr << "[Handler] Error: " << response->errorMessage << std::endl;
        }
        
        return response;
    }

} // namespace mpc_engine::coordinator::handlers::wallet