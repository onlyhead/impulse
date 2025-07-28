#pragma once

#include <concord/core/types.hpp>
#include <concord/geographic/crs/datum.hpp>

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

class Message {
  public:
    virtual ~Message() = default;
    virtual void serialize(char *buffer) const = 0;
    virtual void deserialize(const char *buffer) = 0;
    virtual uint32_t get_size() const = 0;
    virtual std::string to_string() const = 0;
    virtual void set_timestamp(uint64_t timestamp) = 0;
};

struct __attribute__((packed)) Discovery : public Message {
    uint64_t timestamp;
    uint64_t join_time;
    concord::Datum zero_ref;
    bool orchestrator;
    int32_t capability_index;

    inline void serialize(char *buffer) const override { memcpy(buffer, this, sizeof(Discovery)); }
    inline void deserialize(const char *buffer) override { memcpy(this, buffer, sizeof(Discovery)); }
    inline uint32_t get_size() const override { return sizeof(Discovery); }
    inline std::string to_string() const override {
        auto time_t_val = static_cast<time_t>(join_time / 1000);
        auto tm_val = *std::localtime(&time_t_val);
        char time_str[9];
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_val);
        return "AgentMessage{capability=" + std::to_string(capability_index) +
               ", orchestrator=" + (orchestrator ? "true" : "false") + ", joined=" + std::string(time_str) + "}";
    }
    inline void set_timestamp(uint64_t timestamp) override { this->timestamp = timestamp; }
};

struct __attribute__((packed)) Position : public Message {
    uint64_t timestamp;
    concord::Pose pose;

    inline void serialize(char *buffer) const override { memcpy(buffer, this, sizeof(Position)); }
    inline void deserialize(const char *buffer) override { memcpy(this, buffer, sizeof(Position)); }
    inline uint32_t get_size() const override { return sizeof(Position); }
    inline std::string to_string() const override {
        return "Position{pose={point=(" + std::to_string(pose.point.x) + "," + std::to_string(pose.point.y) + "," +
               std::to_string(pose.point.z) + "), angle=(roll=" + std::to_string(pose.angle.roll) +
               ",pitch=" + std::to_string(pose.angle.pitch) + ",yaw=" + std::to_string(pose.angle.yaw) +
               ")}, timestamp=" + std::to_string(timestamp) + "}";
    }
    inline void set_timestamp(uint64_t timestamp) override { this->timestamp = timestamp; }
};

enum struct TransportType : uint8_t {
    dds = 0,
    zenoh = 1,
    zeromq = 2,
    mqtt = 3,
};

enum struct SerializationType : uint8_t {
    ros = 0,
    capnproto = 1,
    flatbuffers = 2,
    json = 3,
    protobuf = 4,
};

struct __attribute__((packed)) Communication : public Message {
    uint64_t timestamp;
    TransportType transport_type;
    SerializationType serialization_type;

    inline void serialize(char *buffer) const override { memcpy(buffer, this, sizeof(Communication)); }
    inline void deserialize(const char *buffer) override { memcpy(this, buffer, sizeof(Communication)); }
    inline uint32_t get_size() const override { return sizeof(Communication); }
    inline std::string to_string() const override {
        std::string transport_type_str;
        switch (transport_type) {
        case TransportType::dds:
            transport_type_str = "dds";
            break;
        case TransportType::zenoh:
            transport_type_str = "zenoh";
            break;
        case TransportType::zeromq:
            transport_type_str = "zeromq";
            break;
        case TransportType::mqtt:
            transport_type_str = "mqtt";
            break;
        default:
            transport_type_str = "unknown";
            break;
        }

        std::string serialization_type_str;
        switch (serialization_type) {
        case SerializationType::ros:
            serialization_type_str = "ros";
            break;
        case SerializationType::capnproto:
            serialization_type_str = "capnproto";
            break;
        case SerializationType::flatbuffers:
            serialization_type_str = "flatbuffers";
            break;
        case SerializationType::json:
            serialization_type_str = "json";
            break;
        case SerializationType::protobuf:
            serialization_type_str = "protobuf";
            break;
        default:
            serialization_type_str = "unknown";
            break;
        }

        return "Communication{transport_type=" + transport_type_str + ", serialization_type=" + serialization_type_str +
               "}";
    }
    inline void set_timestamp(uint64_t timestamp) override { this->timestamp = timestamp; }
};
