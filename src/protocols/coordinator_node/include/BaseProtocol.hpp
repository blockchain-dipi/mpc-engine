// src/protocols/coordinator_node/include/BaseProtocol.hpp
#pragma once
#include "protocols/coordinator_node/include/MessageTypes.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace mpc_engine::protocol::coordinator_node
{
    struct BaseRequest 
    {
        MessageType messageType;
        std::string uid;
        std::string sendTime;

        BaseRequest(MessageType type) : messageType(type) {}
        virtual ~BaseRequest() = default;

        // 비즈니스 로직 검증 (서브클래스에서 override)
        virtual ValidationResult ValidateFields() const 
        {
            if (uid.empty()) {
                return ValidationResult::CORRUPTED_DATA;
            }
            return ValidationResult::OK;
        }
    };

    struct BaseResponse 
    {
        MessageType messageType;
        bool success = false;
        std::string errorMessage;

        BaseResponse(MessageType type) : messageType(type) {}
        virtual ~BaseResponse() = default;
    };
}