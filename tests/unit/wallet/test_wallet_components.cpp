// tests/unit/wallet/test_wallet_components.cpp
#include "coordinator/network/wallet_server/include/WalletConnectionInfo.hpp"
#include "coordinator/handlers/wallet/include/WalletMessageRouter.hpp"
#include "coordinator/network/wallet_server/include/WalletServerManager.hpp"
#include "coordinator/handlers/wallet/include/WalletSigningHandler.hpp"
#include "types/WalletMessageType.hpp"
#include "protocols/coordinator_wallet/include/WalletSigningProtocol.hpp"
#include "common/network/tls/include/TlsContext.hpp"
#include <iostream>
#include <cassert>

using namespace mpc_engine::coordinator::network;
using namespace mpc_engine::coordinator::handlers::wallet;
using namespace mpc_engine::protocol::coordinator_wallet;
using namespace mpc_engine::network::tls;

void PrintTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

// ========================================
// Test 1: WalletConnectionInfo URL 파싱
// ========================================
bool TestUrlParsing() {
    std::cout << "\n=== Test 1: URL Parsing ===" << std::endl;
    
    WalletConnectionInfo info;
    
    // Test 1-1: 기본 URL
    info = WalletConnectionInfo();
    assert(info.ParseUrl("https://wallet.example.com/api/v1"));
    assert(info.host == "wallet.example.com");
    assert(info.port == 443);
    assert(info.path_prefix == "/api/v1");
    std::cout << "  ✓ Basic URL parsing" << std::endl;
    
    // Test 1-2: 포트 지정
    info = WalletConnectionInfo();
    assert(info.ParseUrl("https://localhost:8443/api"));
    assert(info.host == "localhost");
    assert(info.port == 8443);
    assert(info.path_prefix == "/api");
    std::cout << "  ✓ URL with custom port" << std::endl;
    
    // Test 1-3: HTTP (443 아닌 경우)
    info = WalletConnectionInfo();
    assert(info.ParseUrl("http://localhost:3000/v1"));
    assert(info.host == "localhost");
    assert(info.port == 3000);
    assert(info.path_prefix == "/v1");
    std::cout << "  ✓ HTTP URL" << std::endl;
    
    // Test 1-4: 경로 없음
    info = WalletConnectionInfo();
    assert(info.ParseUrl("https://api.wallet.io"));
    assert(info.host == "api.wallet.io");
    assert(info.port == 443);
    assert(info.path_prefix == "");
    std::cout << "  ✓ URL without path" << std::endl;
    
    // Test 1-5: 잘못된 URL
    info = WalletConnectionInfo();
    assert(!info.ParseUrl("invalid_url"));
    std::cout << "  ✓ Invalid URL rejection" << std::endl;
    
    return true;
}

// ========================================
// Test 2: JSON 직렬화/역직렬화
// ========================================
bool TestJsonSerialization() {
    std::cout << "\n=== Test 2: JSON Serialization ===" << std::endl;
    
    // Test 2-1: WalletSigningRequest → JSON
    WalletSigningRequest request;
    request.messageType = WalletMessageType::SIGNING_REQUEST;
    request.requestId = "req_123";
    request.timestamp = "1234567890";
    request.coordinatorId = "coord_1";
    request.keyId = "wallet_key_abc";
    request.transactionData = "0xabcdef";
    request.threshold = 2;
    request.totalShards = 3;
    
    std::string json = request.ToJson();
    
    assert(!json.empty());
    assert(json.find("wallet_key_abc") != std::string::npos);
    assert(json.find("0xabcdef") != std::string::npos);
    std::cout << "  ✓ Request serialization" << std::endl;
    std::cout << "    JSON: " << json.substr(0, 80) << "..." << std::endl;
    
    // Test 2-2: JSON → WalletSigningResponse
    std::string response_json = R"({
        "messageType": 1001,
        "success": true,
        "requestId": "req_123",
        "timestamp": "1234567890",
        "keyId": "wallet_key_abc",
        "finalSignature": "0xfinal_sig",
        "shardSignatures": ["0xshard0", "0xshard1", "0xshard2"],
        "successfulShards": 3
    })";
    
    WalletSigningResponse response;
    bool parse_success = response.FromJson(response_json);
    
    assert(parse_success);
    assert(response.success);
    assert(response.keyId == "wallet_key_abc");
    assert(response.finalSignature == "0xfinal_sig");
    assert(response.shardSignatures.size() == 3);
    assert(response.successfulShards == 3);
    std::cout << "  ✓ Response deserialization" << std::endl;
    
    // Test 2-3: 에러 응답
    std::string error_json = R"({
        "messageType": 1001,
        "success": false,
        "errorMessage": "Key not found"
    })";
    
    WalletSigningResponse error_response;
    assert(error_response.FromJson(error_json));
    assert(!error_response.success);
    assert(error_response.errorMessage == "Key not found");
    std::cout << "  ✓ Error response parsing" << std::endl;
    
    return true;
}

// ========================================
// Test 3: WalletMessageRouter
// ========================================
bool TestProtocolRouter() {
    std::cout << "\n=== Test 3: Protocol Router ===" << std::endl;
    
    WalletMessageRouter& router = WalletMessageRouter::Instance();
    
    // 초기화
    assert(router.Initialize());
    std::cout << "  ✓ Router initialized" << std::endl;
    
    // Signing Request 처리
    WalletSigningRequest request;
    request.keyId = "test_key";
    request.transactionData = "0x123";
    request.threshold = 2;
    request.totalShards = 3;
    
    auto response = router.ProcessMessage(
        WalletMessageType::SIGNING_REQUEST,
        &request
    );
    
    assert(response != nullptr);
    assert(response->success);
    std::cout << "  ✓ SIGNING_REQUEST routed correctly" << std::endl;
    
    // 잘못된 타입
    auto invalid_response = router.ProcessMessage(
        static_cast<WalletMessageType>(9999),
        &request
    );
    
    assert(invalid_response == nullptr);
    std::cout << "  ✓ Invalid message type rejected" << std::endl;
    
    return true;
}

// ========================================
// Test 4: WalletSigningHandler
// ========================================
bool TestSigningHandler() {
    std::cout << "\n=== Test 4: Signing Handler ===" << std::endl;
    
    WalletSigningRequest request;
    request.requestId = "handler_test_001";
    request.keyId = "handler_key_123";
    request.transactionData = "0xhandler_tx_data";
    request.threshold = 2;
    request.totalShards = 3;
    
    auto response = HandleWalletSigningRequest(&request);
    
    assert(response != nullptr);
    
    // WalletSigningResponse로 캐스팅
    WalletSigningResponse* signing_response = 
        dynamic_cast<WalletSigningResponse*>(response.get());
    
    assert(signing_response != nullptr);
    assert(signing_response->success);
    assert(signing_response->keyId == request.keyId);
    assert(!signing_response->finalSignature.empty());
    assert(signing_response->shardSignatures.size() == request.totalShards);
    
    std::cout << "  ✓ Handler executed successfully" << std::endl;
    std::cout << "    Final Signature: " << signing_response->finalSignature.substr(0, 50) << "..." << std::endl;
    std::cout << "    Shard Count: " << signing_response->shardSignatures.size() << std::endl;
    
    return true;
}

// ========================================
// Test 5: WalletServerManager 초기화
// ========================================
bool TestManagerInitialization() {
    std::cout << "\n=== Test 5: WalletServerManager Initialization ===" << std::endl;
    
    // TLS Context 생성
    TlsConfig tls_config;
    tls_config.mode = TlsMode::CLIENT;
    
    TlsContext tls_ctx;
    if (!tls_ctx.Initialize(tls_config)) {
        std::cerr << "Failed to initialize TLS context" << std::endl;
        return false;
    }
    std::cout << "  ✓ TLS Context initialized" << std::endl;
    
    // WalletServerManager 초기화
    WalletServerManager& manager = WalletServerManager::Instance();
    
    std::string wallet_url = "https://localhost:3000/api/v1";
    std::string auth_token = "Bearer test_token_12345";
    
    bool init_result = manager.Initialize(wallet_url, auth_token, tls_ctx);
    
    assert(init_result);
    assert(manager.IsInitialized());
    std::cout << "  ✓ WalletServerManager initialized" << std::endl;
    
    // 연결 정보 확인
    WalletConnectionInfo info = manager.GetConnectionInfo();
    assert(info.host == "localhost");
    assert(info.port == 3000);
    assert(info.path_prefix == "/api/v1");
    assert(info.auth_token == auth_token);
    std::cout << "  ✓ Connection info validated" << std::endl;
    
    return true;
}

// ========================================
// Main
// ========================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Wallet Components Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        PrintTestResult("URL Parsing", TestUrlParsing());
        PrintTestResult("JSON Serialization", TestJsonSerialization());
        PrintTestResult("Protocol Router", TestProtocolRouter());
        PrintTestResult("Signing Handler", TestSigningHandler());
        PrintTestResult("Manager Initialization", TestManagerInitialization());
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  All tests passed!" << std::endl;
        std::cout << "  Ready for integration with Wallet Server" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed: " << e.what() << std::endl;
        return 1;
    }
}