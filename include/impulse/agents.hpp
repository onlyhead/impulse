#pragma once

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
    
    void handle_discovery_message(const Discovery& msg, const std::string& from_addr);
    bool should_share_info_with(int32_t other_capability);

  public:
    Agent(const std::string& name, LanInterface* lan_interface, int32_t capability = 75);
    ~Agent();

    bool start();
    void stop();
    void print_status() const;
    void send_discovery();
    
    int32_t get_capability() const;
    std::string get_ipv6() const;
};

inline Agent::Agent(const std::string& name, LanInterface* lan_interface, int32_t capability)
    : name_(name), transport_(name, lan_interface, capability) {
    
    // Set up message handler for discovery messages
    transport_.set_message_handler([this](const Discovery& msg, const std::string& from_addr) {
        this->handle_discovery_message(msg, from_addr);
    });
}

inline Agent::~Agent() {
    stop();
}

inline bool Agent::start() {
    std::cout << name_ << " starting agent discovery" << std::endl;
    
    // Add self to known agents list
    std::lock_guard<std::mutex> lock(agents_mutex_);
    Discovery self_msg = {};
    self_msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    self_msg.join_time = self_msg.timestamp;
    self_msg.capability_index = transport_.get_capability();
    strncpy(self_msg.ipv6, get_ipv6().c_str(), 45);
    known_agents_[get_ipv6()] = self_msg;
    
    return transport_.start();
}

inline void Agent::stop() {
    transport_.stop();
}

inline void Agent::send_discovery() {
    Discovery msg = {};
    msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    msg.join_time = msg.timestamp;
    
    msg.orchestrator = false;
    msg.zero_ref = {40.7128, -74.0060, 0.0};
    msg.capability_index = transport_.get_capability();
    strncpy(msg.ipv6, get_ipv6().c_str(), 45);
    
    transport_.send(msg);
}

inline void Agent::handle_discovery_message(const Discovery& msg, const std::string& from_addr) {
    if (should_share_info_with(msg.capability_index)) {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        std::string agent_ipv6(msg.ipv6);
        if (known_agents_.find(agent_ipv6) == known_agents_.end()) {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            auto join_time_seconds = (now - msg.join_time) / 1000;
            std::cout << name_ << " discovered: " << agent_ipv6 << " cap:" << msg.capability_index 
                      << " joined " << join_time_seconds << "s ago from " << from_addr << std::endl;
        }
        known_agents_[agent_ipv6] = msg;
    }
}

inline bool Agent::should_share_info_with(int32_t other_capability) {
    int32_t capability = transport_.get_capability();
    if (capability >= 90 || other_capability >= 90) {
        return true;
    } else if (capability >= 60 && other_capability >= 60) {
        return true;
    } else if (capability >= 50 && other_capability >= 50) {
        return true;
    }
    return capability >= 25 && other_capability >= 25;
}

inline void Agent::print_status() const {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    std::cout << "\n" << name_ << " Status:" << std::endl;
    std::cout << "  IPv6: " << get_ipv6() << std::endl;
    std::cout << "  Capability: " << transport_.get_capability() << "/100" << std::endl;
    std::cout << "  Known agents: " << known_agents_.size() << std::endl;
    for (const auto& [ipv6, agent] : known_agents_) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto join_time_seconds = (now - agent.join_time) / 1000;
        std::cout << "    - " << agent.to_string() << " joined " << join_time_seconds << "s ago" << std::endl;
    }
}

inline int32_t Agent::get_capability() const {
    return transport_.get_capability();
}

inline std::string Agent::get_ipv6() const {
    return transport_.get_ipv6();
}