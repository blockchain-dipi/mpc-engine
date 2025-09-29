// src/protocols/coordinator_node/include/MessageTypes.hpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

namespace mpc_engine::protocol::coordinator_node
{
    struct MessageHeader 
    {
        uint32_t magic = 0x4D504345;      // "MPCE"
        uint16_t version = 0x0001;
        uint16_t message_type;
        uint32_t body_length;
        uint32_t checksum;
        uint64_t timestamp;
        char session_id[16];

        MessageHeader() : message_type(0), body_length(0), checksum(0), timestamp(0) 
        {
            memset(session_id, 0, sizeof(session_id));
        }

        MessageHeader(uint16_t type, uint32_t length) 
            : message_type(type), body_length(length), checksum(0), timestamp(0) 
        {
            memset(session_id, 0, sizeof(session_id));
        }

        bool IsValid() const 
        {
            return magic == 0x4D504345 && version == 0x0001 && body_length <= (1024 * 1024);
        }

        void SetSessionId(const std::string& id) 
        {
            memset(session_id, 0, sizeof(session_id));
            strncpy(session_id, id.c_str(), sizeof(session_id) - 1);
        }

        std::string GetSessionId() const 
        {
            return std::string(session_id, strnlen(session_id, sizeof(session_id)));
        }
    } __attribute__((packed));

    enum class MessageType : uint32_t 
    {
        SIGNING_REQUEST = 0,
        MAX_MESSAGE_TYPE
    };

    struct BaseRequest 
    {
        MessageType messageType;
        std::string uid;
        std::string sendTime;

        BaseRequest(MessageType type) : messageType(type) {}
        virtual ~BaseRequest() = default;
    };

    struct BaseResponse 
    {
        MessageType messageType;
        bool success = false;
        std::string errorMessage;

        BaseResponse(MessageType type) : messageType(type) {}
        virtual ~BaseResponse() = default;
    };

    struct NetworkMessage 
    {
        MessageHeader header;
        std::vector<uint8_t> body;

        NetworkMessage() = default;
        NetworkMessage(uint16_t type, const std::vector<uint8_t>& data) 
            : header(type, static_cast<uint32_t>(data.size())), body(data) {}
        NetworkMessage(uint16_t type, const std::string& data) 
            : header(type, static_cast<uint32_t>(data.size())) {
            body.assign(data.begin(), data.end());
        }

        std::string GetBodyAsString() const 
        {
            return std::string(body.begin(), body.end());
        }

        bool IsValid() const 
        {
            return header.IsValid() && body.size() == header.body_length;
        }

        size_t GetTotalSize() const 
        {
            return sizeof(MessageHeader) + body.size();
        }
    };
}