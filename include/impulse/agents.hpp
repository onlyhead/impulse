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
    : name_(name), transport_(name, network_interface, capability, true, std::chrono::milliseconds(1000)) {

    // Set up message handler for discovery messages
    transport_.set_message_handler(
        [this](const Discovery &msg, const std::string &from_addr) { this->handle_discovery_message(msg, from_addr); });

    // Add self to known agents list and set up continuous broadcast
    std::lock_guard<std::mutex> lock(agents_mutex_);
    Discovery self_msg = {};
    auto now_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    self_msg.timestamp = now_time;
    self_msg.join_time = now_time;  // Set join_time once at startup
    self_msg.orchestrator = false;
    self_msg.zero_ref = {40.7128, -74.0060, 0.0};
    self_msg.capability_index = transport_.get_capability();
    strncpy(self_msg.ipv6, transport_.get_address().c_str(), 45);
    known_agents_[transport_.get_address()] = self_msg;

    // Set this message for continuous broadcasting
    transport_.set_broadcast_message(self_msg);
    transport_.start();
}

inline Agent::~Agent() { transport_.stop(); }

inline void Agent::send_discovery() {
    Discovery msg = {};
    auto now_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    msg.timestamp = now_time;
    // Keep the original join_time from when robot started
    std::lock_guard<std::mutex> lock(agents_mutex_);
    auto self_agent = known_agents_.find(transport_.get_address());
    if (self_agent != known_agents_.end()) {
        msg.join_time = self_agent->second.join_time;
    } else {
        msg.join_time = now_time;  // Fallback if not found
    }
    msg.orchestrator = false;
    msg.zero_ref = {40.7128, -74.0060, 0.0};
    msg.capability_index = transport_.get_capability();
    strncpy(msg.ipv6, transport_.get_address().c_str(), 45);
    transport_.set_broadcast_message(msg);
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
