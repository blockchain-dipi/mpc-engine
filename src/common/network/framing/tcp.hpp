// src/types/MessageTypes.hpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace mpc_engine::network::framing
{
    // 보안 상수
    constexpr uint32_t MAGIC_NUMBER = 0x4D504345;  // "MPCE"
    constexpr uint16_t PROTOCOL_VERSION = 0x0001;
    constexpr uint32_t MAX_BODY_SIZE = 1024 * 1024;  // 1MB
    constexpr uint32_t MIN_BODY_SIZE = 0;

    // 검증 결과
    enum class ValidationResult : uint8_t 
    {
        OK = 0,
        INVALID_MAGIC = 1,
        INVALID_VERSION = 2,
        BODY_TOO_LARGE = 3,
        BODY_SIZE_MISMATCH = 4,
        INVALID_MESSAGE_TYPE = 5,
        CHECKSUM_MISMATCH = 6,
        CORRUPTED_DATA = 7
    };

    inline const char* ValidationResultToString(ValidationResult result) 
    {
        switch (result) {
            case ValidationResult::OK: return "OK";
            case ValidationResult::INVALID_MAGIC: return "Invalid magic number";
            case ValidationResult::INVALID_VERSION: return "Invalid version";
            case ValidationResult::BODY_TOO_LARGE: return "Body too large";
            case ValidationResult::BODY_SIZE_MISMATCH: return "Body size mismatch";
            case ValidationResult::INVALID_MESSAGE_TYPE: return "Invalid message type";
            case ValidationResult::CHECKSUM_MISMATCH: return "Checksum mismatch";
            case ValidationResult::CORRUPTED_DATA: return "Corrupted data";
            default: return "Unknown error";
        }
    }
    
    struct MessageHeader 
    {
        uint32_t magic = MAGIC_NUMBER;
        uint16_t version = PROTOCOL_VERSION;
        uint16_t message_type;
        uint32_t body_length;
        uint32_t checksum;
        uint64_t timestamp;
        uint64_t request_id;

        MessageHeader() 
            : message_type(0), body_length(0), checksum(0), timestamp(0), request_id(0) {}

        MessageHeader(uint16_t type, uint32_t length) 
            : message_type(type), body_length(length), checksum(0), timestamp(0), request_id(0) {}

        // 기본 헤더 검증
        ValidationResult ValidateBasic() const 
        {
            // 1. Magic number 검증
            if (magic != MAGIC_NUMBER) {
                return ValidationResult::INVALID_MAGIC;
            }

            // 2. Version 검증
            if (version != PROTOCOL_VERSION) {
                return ValidationResult::INVALID_VERSION;
            }

            // 3. Body length 범위 검증
            if (body_length > MAX_BODY_SIZE) {
                return ValidationResult::BODY_TOO_LARGE;
            }

            return ValidationResult::OK;
        }

        // 레거시 호환
        bool IsValid() const 
        {
            return ValidateBasic() == ValidationResult::OK;
        }

        // Checksum 계산 (간단한 XOR 체크섬)
        static uint32_t ComputeChecksum(const std::vector<uint8_t>& data) 
        {
            uint32_t checksum = 0;
            for (size_t i = 0; i < data.size(); i += 4) {
                uint32_t chunk = 0;
                size_t remaining = std::min(size_t(4), data.size() - i);
                memcpy(&chunk, &data[i], remaining);
                checksum ^= chunk;
            }
            return checksum;
        }
    } __attribute__((packed));

    struct NetworkMessage 
    {
        MessageHeader header;
        std::vector<uint8_t> body;

        NetworkMessage() = default;
        
        NetworkMessage(uint16_t type, const std::vector<uint8_t>& data) 
            : header(type, static_cast<uint32_t>(data.size())), body(data) 
        {
            header.checksum = MessageHeader::ComputeChecksum(body);
        }
        
        NetworkMessage(uint16_t type, const std::string& data) 
            : header(type, static_cast<uint32_t>(data.size())) 
        {
            body.assign(data.begin(), data.end());
            header.checksum = MessageHeader::ComputeChecksum(body);
        }

        std::string GetBodyAsString() const 
        {
            return std::string(body.begin(), body.end());
        }

        // 전체 메시지 검증
        ValidationResult Validate() const 
        {
            // 1. 헤더 기본 검증
            ValidationResult result = header.ValidateBasic();
            if (result != ValidationResult::OK) {
                return result;
            }

            // 2. Body 크기 일치 검증
            if (body.size() != header.body_length) {
                return ValidationResult::BODY_SIZE_MISMATCH;
            }

            // 3. Checksum 검증
            uint32_t computed = MessageHeader::ComputeChecksum(body);
            if (computed != header.checksum) {
                return ValidationResult::CHECKSUM_MISMATCH;
            }

            return ValidationResult::OK;
        }

        bool IsValid() const 
        {
            return Validate() == ValidationResult::OK;
        }

        size_t GetTotalSize() const 
        {
            return sizeof(MessageHeader) + body.size();
        }
    };
} // namespace mpc_engine::network::framing