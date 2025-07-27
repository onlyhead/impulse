#pragma once

#include <concord/core/types.hpp>
#include <concord/geographic/crs/datum.hpp>

#include <cstdint>
#include <cstring>
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

struct Discovery : public Message {
    uint64_t timestamp;
    uint64_t join_time;
    char ipv6[46];
    concord::Datum zero_ref;
    bool orchestrator;
    int32_t capability_index;

    inline void serialize(char *buffer) const override { memcpy(buffer, this, sizeof(Discovery)); }
    inline void deserialize(const char *buffer) override { memcpy(this, buffer, sizeof(Discovery)); }
    inline uint32_t get_size() const override { return sizeof(Discovery); }
    inline std::string to_string() const override {
        return "AgentMessage{ipv6=" + std::string(ipv6) + ", capability=" + std::to_string(capability_index) +
               ", orchestrator=" + (orchestrator ? "true" : "false") + "}";
    }
    inline void set_timestamp(uint64_t timestamp) override { this->timestamp = timestamp; }
};

struct Position : public Message {
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
