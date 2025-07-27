#pragma once

#include "impulse/network/lan.hpp"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <random>
#include <thread>

struct GeoPoint {
    double latitude;
    double longitude;
    double altitude;
};

struct AgentMessage {
    uint64_t timestamp;
    char public_key[64];
    char uuid[37];
    bool orchestrator;
    GeoPoint zero_ref;
    char participant_uuids[10][37];
    int32_t capability_index;
    char ipv6_addresses[3][46];
    uint32_t robot_id;
    char robot_name[32];

    inline void serialize(char *buffer) const { memcpy(buffer, this, sizeof(AgentMessage)); }

    inline static AgentMessage deserialize(const char *buffer) {
        AgentMessage msg;
        memcpy(&msg, buffer, sizeof(AgentMessage));
        return msg;
    }
};

class Aris {
  private:
    std::string name_;
    std::string uuid_;
    uint32_t id_;
    int32_t capability_index_;
    LanInterface *lan_interface_;

    std::thread discovery_thread_;
    std::atomic<bool> running_;

    std::map<std::string, AgentMessage> known_robots_;
    mutable std::mutex robots_mutex_;

    void discovery_loop();
    void send_agent_message();
    void handle_incoming_message(const std::string &message, const std::string &from_addr);
    bool should_share_info_with(int32_t other_capability);
    std::string generate_uuid();

  public:
    Aris(const std::string &name, uint32_t id, LanInterface *lan_interface, int32_t capability = 75);
    ~Aris();

    bool start();
    void stop();
    void print_status() const;
    int32_t get_capability() const;
};

inline Aris::Aris(const std::string &name, uint32_t id, LanInterface *lan_interface, int32_t capability)
    : name_(name), id_(id), capability_index_(capability), lan_interface_(lan_interface), running_(false) {

    uuid_ = generate_uuid();
}

inline Aris::~Aris() { stop(); }

inline bool Aris::start() {
    std::cout << name_ << " (" << uuid_ << ") starting ARIS discovery" << std::endl;

    // Add self to known robots list
    {
        std::lock_guard<std::mutex> lock(robots_mutex_);
        AgentMessage self_msg = {};
        self_msg.timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        strncpy(self_msg.uuid, uuid_.c_str(), sizeof(self_msg.uuid) - 1);
        self_msg.capability_index = capability_index_;
        self_msg.robot_id = id_;
        strncpy(self_msg.robot_name, name_.c_str(), sizeof(self_msg.robot_name) - 1);
        strncpy(self_msg.ipv6_addresses[0], lan_interface_->get_ipv6().c_str(), 45);
        strncpy(self_msg.ipv6_addresses[1], "", 45);
        strncpy(self_msg.ipv6_addresses[2], "", 45);
        known_robots_[uuid_] = self_msg;
    }

    // Register callback to receive messages from LAN interface
    lan_interface_->set_message_callback(
        [this](const std::string &message, const std::string &from_addr, uint16_t from_port) {
            this->handle_incoming_message(message, from_addr);
        });

    running_ = true;
    discovery_thread_ = std::thread(&Aris::discovery_loop, this);

    return true;
}

inline void Aris::stop() {
    running_ = false;
    if (discovery_thread_.joinable()) {
        discovery_thread_.join();
    }
}

inline void Aris::discovery_loop() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(5, 15);
    auto listen_duration = std::chrono::seconds(dis(gen));
    auto listen_start = std::chrono::steady_clock::now();

    std::cout << name_ << " listening for " << listen_duration.count() << " seconds..." << std::endl;

    while (running_ && (std::chrono::steady_clock::now() - listen_start) < listen_duration) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!running_) return;

    std::cout << name_ << " starting discovery announcements" << std::endl;

    while (running_) {
        send_agent_message();

        for (int i = 0; i < 20 && running_; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

inline void Aris::send_agent_message() {
    AgentMessage msg = {};
    msg.timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    strncpy(msg.public_key, "ed25519_key_placeholder", sizeof(msg.public_key) - 1);
    strncpy(msg.uuid, uuid_.c_str(), sizeof(msg.uuid) - 1);
    msg.orchestrator = false;
    msg.zero_ref = {40.7128, -74.0060, 0.0};
    msg.capability_index = capability_index_;

    strncpy(msg.ipv6_addresses[0], lan_interface_->get_ipv6().c_str(), 45);
    strncpy(msg.ipv6_addresses[1], "", 45);
    strncpy(msg.ipv6_addresses[2], "", 45);

    msg.robot_id = id_;
    strncpy(msg.robot_name, name_.c_str(), sizeof(msg.robot_name) - 1);

    // Serialize and send via LAN interface
    char buffer[sizeof(AgentMessage)];
    msg.serialize(buffer);
    std::string message(buffer, sizeof(AgentMessage));

    lan_interface_->multicast_message(message);
}

inline bool Aris::should_share_info_with(int32_t other_capability) {
    if (capability_index_ >= 90 || other_capability >= 90) {
        return true;
    } else if (capability_index_ >= 60 && other_capability >= 60) {
        return true;
    } else if (capability_index_ >= 50 && other_capability >= 50) {
        return true;
    }
    return capability_index_ >= 25 && other_capability >= 25;
}

inline std::string Aris::generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str), "%08x-%04x-%04x-%04x-%012lx", id_, 4096, 16384,
             dis(gen) * 4096 + dis(gen) * 256 + dis(gen) * 16 + dis(gen),
             (unsigned long)(std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count() &
                             0xFFFFFFFFFFFF));

    return std::string(uuid_str);
}

inline void Aris::print_status() const {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    std::cout << "\n" << name_ << " Status:" << std::endl;
    std::cout << "  UUID: " << uuid_ << std::endl;
    std::cout << "  Robot ID: " << id_ << std::endl;
    std::cout << "  IPv6: " << lan_interface_->get_ipv6() << std::endl;
    std::cout << "  Capability: " << capability_index_ << "/100" << std::endl;
    std::cout << "  Known robots: " << known_robots_.size() << std::endl;
    for (const auto &[id, agent] : known_robots_) {
        std::string ipv6_addr = agent.ipv6_addresses[0];
        if (ipv6_addr.empty()) ipv6_addr = "unknown";
        std::cout << "    - " << agent.robot_name << " cap:" << agent.capability_index << " ipv6:" << ipv6_addr
                  << " uuid:" << agent.uuid << std::endl;
    }
}

inline void Aris::handle_incoming_message(const std::string &message, const std::string &from_addr) {
    if (message.size() == sizeof(AgentMessage)) {
        AgentMessage msg = AgentMessage::deserialize(message.c_str());

        if (should_share_info_with(msg.capability_index)) {
            std::lock_guard<std::mutex> lock(robots_mutex_);
            std::string robot_uuid(msg.uuid);
            if (known_robots_.find(robot_uuid) == known_robots_.end()) {
                std::cout << name_ << " discovered: " << msg.robot_name << " cap:" << msg.capability_index << " from "
                          << from_addr << std::endl;
            }
            known_robots_[robot_uuid] = msg;
        }
    }
}

inline int32_t Aris::get_capability() const { return capability_index_; }
