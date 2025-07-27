#pragma once

#include "impulse/network/interface.hpp"
#include "impulse/protocol/message.hpp"
#include "impulse/protocol/transport.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <string>

class Agent {
  private:
    std::string name_;
    Transport<Discovery> transport_;
    std::map<std::string, Discovery> known_agents_;
    mutable std::mutex agents_mutex_;

  public:
    Agent(const std::string &name, NetworkInterface *network_interface, int32_t capability = 75);
    ~Agent();

    void print_status() const;
    void send_discovery();

  private:
    void handle_discovery_message(const Discovery &msg, const std::string &from_addr);
};

inline Agent::Agent(const std::string &name, NetworkInterface *network_interface, int32_t capability)
    : name_(name), transport_(name, network_interface, capability) {

    // Set up message handler for discovery messages
    transport_.set_message_handler(
        [this](const Discovery &msg, const std::string &from_addr) { this->handle_discovery_message(msg, from_addr); });

    // Add self to known agents list
    std::lock_guard<std::mutex> lock(agents_mutex_);
    Discovery self_msg = {};
    self_msg.timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    self_msg.join_time = self_msg.timestamp;
    self_msg.capability_index = transport_.get_capability();
    strncpy(self_msg.ipv6, transport_.get_address().c_str(), 45);
    known_agents_[transport_.get_address()] = self_msg;

    transport_.start();
}

inline Agent::~Agent() { transport_.stop(); }

inline void Agent::send_discovery() {
    Discovery msg = {};
    msg.timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    msg.join_time = msg.timestamp;

    msg.orchestrator = false;
    msg.zero_ref = {40.7128, -74.0060, 0.0};
    msg.capability_index = transport_.get_capability();
    strncpy(msg.ipv6, transport_.get_address().c_str(), 45);

    transport_.send(msg);
}

inline void Agent::handle_discovery_message(const Discovery &msg, const std::string &from_addr) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    std::string agent_ipv6(msg.ipv6);
    if (known_agents_.find(agent_ipv6) == known_agents_.end()) {
        auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        auto join_time_seconds = (now - msg.join_time) / 1000;
    }
    known_agents_[agent_ipv6] = msg;
}

inline void Agent::print_status() const {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    for (const auto &[ipv6, agent] : known_agents_) {
        auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        auto join_time_seconds = (now - agent.join_time) / 1000;
        std::cout << "    - " << agent.to_string() << " joined " << join_time_seconds << "s ago" << std::endl;
    }
}
