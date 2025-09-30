// tests/integration/message_validation_test.cpp
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <iostream>
#include <cassert>

using namespace mpc_engine::protocol::coordinator_node;

void TestInvalidMagic() {
    std::cout << "\n=== Test: Invalid Magic Number ===" << std::endl;
    
    NetworkMessage msg(0, "test");
    msg.header.magic = 0xDEADBEEF;  // 잘못된 magic
    
    ValidationResult result = msg.Validate();
    assert(result == ValidationResult::INVALID_MAGIC);
    std::cout << "✓ Detected invalid magic: " << ToString(result) << std::endl;
}

void TestInvalidVersion() {
    std::cout << "\n=== Test: Invalid Version ===" << std::endl;
    
    NetworkMessage msg(0, "test");
    msg.header.version = 0x9999;  // 잘못된 버전
    
    ValidationResult result = msg.Validate();
    assert(result == ValidationResult::INVALID_VERSION);
    std::cout << "✓ Detected invalid version: " << ToString(result) << std::endl;
}

void TestBodyTooLarge() {
    std::cout << "\n=== Test: Body Too Large ===" << std::endl;
    
    NetworkMessage msg;
    msg.header.magic = MAGIC_NUMBER;
    msg.header.version = PROTOCOL_VERSION;
    msg.header.message_type = 0;
    msg.header.body_length = MAX_BODY_SIZE + 1;  // 너무 큼
    
    ValidationResult result = msg.header.ValidateBasic();
    assert(result == ValidationResult::BODY_TOO_LARGE);
    std::cout << "✓ Detected body too large: " << ToString(result) << std::endl;
}

void TestBodySizeMismatch() {
    std::cout << "\n=== Test: Body Size Mismatch ===" << std::endl;
    
    NetworkMessage msg(0, "test");
    msg.header.body_length = 999;  // body와 다름
    
    ValidationResult result = msg.Validate();
    assert(result == ValidationResult::BODY_SIZE_MISMATCH);
    std::cout << "✓ Detected body size mismatch: " << ToString(result) << std::endl;
}

void TestChecksumMismatch() {
    std::cout << "\n=== Test: Checksum Mismatch ===" << std::endl;
    
    NetworkMessage msg(0, "test");
    msg.header.checksum = 0xFFFFFFFF;  // 잘못된 체크섬
    
    ValidationResult result = msg.Validate();
    assert(result == ValidationResult::CHECKSUM_MISMATCH);
    std::cout << "✓ Detected checksum mismatch: " << ToString(result) << std::endl;
}

void TestInvalidMessageType() {
    std::cout << "\n=== Test: Invalid Message Type ===" << std::endl;
    
    NetworkMessage msg(0, "test");
    msg.header.message_type = 9999;  // 존재하지 않는 타입
    
    ValidationResult result = msg.Validate();
    assert(result == ValidationResult::INVALID_MESSAGE_TYPE);
    std::cout << "✓ Detected invalid message type: " << ToString(result) << std::endl;
}

void TestValidMessage() {
    std::cout << "\n=== Test: Valid Message ===" << std::endl;
    
    NetworkMessage msg(0, "Hello MPC");
    
    ValidationResult result = msg.Validate();
    assert(result == ValidationResult::OK);
    assert(msg.IsValid());
    std::cout << "✓ Valid message accepted" << std::endl;
}

void TestAttackScenarios() {
    std::cout << "\n=== Attack Scenario Tests ===" << std::endl;
    
    // 시나리오 1: 거대 body_length로 메모리 고갈 시도
    {
        std::cout << "\nScenario 1: Memory exhaustion attack" << std::endl;
        NetworkMessage msg;
        msg.header.magic = MAGIC_NUMBER;
        msg.header.version = PROTOCOL_VERSION;
        msg.header.body_length = 0xFFFFFFFF;  // 4GB
        
        ValidationResult result = msg.header.ValidateBasic();
        assert(result == ValidationResult::BODY_TOO_LARGE);
        std::cout << "✓ Blocked memory exhaustion attack" << std::endl;
    }
    
    // 시나리오 2: Integer underflow 시도
    {
        std::cout << "\nScenario 2: Integer underflow attack" << std::endl;
        NetworkMessage msg;
        msg.header.magic = MAGIC_NUMBER;
        msg.header.version = PROTOCOL_VERSION;
        msg.header.body_length = static_cast<uint32_t>(-1);
        
        ValidationResult result = msg.header.ValidateBasic();
        assert(result == ValidationResult::BODY_TOO_LARGE);
        std::cout << "✓ Blocked integer underflow attack" << std::endl;
    }
    
    // 시나리오 3: 같은 길이로 데이터만 변조 (현실적)
    {
        std::cout << "\nScenario 3: Data tampering (same length)" << std::endl;
        NetworkMessage msg(0, "original data");
        
        // body 크기는 유지하면서 내용만 변조
        std::string tampered = "hacked data!!";  // 같은 길이 (13바이트)
        msg.body.assign(tampered.begin(), tampered.end());
        // header.checksum은 그대로 (원본 데이터의 체크섬)
        
        ValidationResult result = msg.Validate();
        assert(result == ValidationResult::CHECKSUM_MISMATCH);
        std::cout << "✓ Detected data tampering" << std::endl;
    }
    
    // 시나리오 4: body_length와 실제 body 크기 불일치
    {
        std::cout << "\nScenario 4: Body size mismatch attack" << std::endl;
        NetworkMessage msg(0, "test");
        msg.header.body_length = 100;  // 실제는 4바이트
        
        ValidationResult result = msg.Validate();
        assert(result == ValidationResult::BODY_SIZE_MISMATCH);
        std::cout << "✓ Detected body size mismatch" << std::endl;
    }
    
    // 시나리오 5: 잘못된 프로토콜 (HTTP 등)
    {
        std::cout << "\nScenario 5: Wrong protocol attack" << std::endl;
        NetworkMessage msg;
        msg.header.magic = 0x47455420;  // "GET " (HTTP)
        msg.header.version = PROTOCOL_VERSION;
        
        ValidationResult result = msg.header.ValidateBasic();
        assert(result == ValidationResult::INVALID_MAGIC);
        std::cout << "✓ Blocked wrong protocol" << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Message Validation Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        TestInvalidMagic();
        TestInvalidVersion();
        TestBodyTooLarge();
        TestBodySizeMismatch();
        TestChecksumMismatch();
        TestInvalidMessageType();
        TestValidMessage();
        TestAttackScenarios();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All validation tests passed! ✓" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}