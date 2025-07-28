#include "impulse/network/interface.hpp"
#include "impulse/network/lan.hpp"
#include "impulse/protocol/message.hpp"
#include "impulse/protocol/transport.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class Agent {
  private:
    std::string name_;
    int32_t capability_index_;
    Transport<Discovery> discovery_;
    std::map<std::string, Discovery> known_agents_;
    mutable std::mutex agents_mutex_;

  public:
    Agent(const std::string &name, NetworkInterface *network_interface, int32_t capability = 75);
    ~Agent();

    void print_status() const;

  private:
    void handle_discovery_message(const Discovery &msg, const std::string &from_addr);
};

inline Agent::Agent(const std::string &name, NetworkInterface *network_interface, int32_t capability)
    : name_(name), capability_index_(capability), discovery_(name, network_interface, true, std::chrono::milliseconds(1000)) {

    // Add self to known agents list and set up continuous broadcast
    std::lock_guard<std::mutex> lock(agents_mutex_);
    Discovery self_msg = {};
    auto now_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    self_msg.timestamp = now_time;
    self_msg.join_time = now_time; // Set join_time once at startup
    self_msg.orchestrator = false;
    self_msg.zero_ref = {40.7128, -74.0060, 0.0};
    self_msg.capability_index = capability_index_;
    strncpy(self_msg.ipv6, discovery_.get_address().c_str(), 45);
    known_agents_[discovery_.get_address()] = self_msg;

    // Set up message handler for discovery messages
    discovery_.set_message_handler(
        [this](const Discovery &msg, const std::string &from_addr) { this->handle_discovery_message(msg, from_addr); });
    // Set this message for continuous broadcasting
    discovery_.set_broadcast_message(self_msg);
    discovery_.start();
}

inline Agent::~Agent() { discovery_.stop(); }

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

std::atomic<bool> should_exit{false};

void signal_handler(int signal) {
    std::cout << "\nCaught signal " << signal << ", requesting shutdown..." << std::endl;
    should_exit = true;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <robot_name>" << std::endl;
        std::cerr << "Example: " << argv[0] << " Tractor-Alpha" << std::endl;
        return 1;
    }

    std::string robot_name = argv[1];
    std::cout << "=== ARIS Robot: " << robot_name << " ===\n" << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    LanInterface lan("eno2");
    if (!lan.start()) {
        std::cerr << "Failed to start LAN interface" << std::endl;
        return 1;
    }

    Agent participant(robot_name, &lan, 75);

    std::cout << "Robot started. Waiting for discovery..." << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    while (!should_exit && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(60)) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::cout << "\n=== Current Network Status ===" << std::endl;
        participant.print_status();
    }

    if (!should_exit) {
        std::cout << "\nDiscovery complete! Press Enter to shutdown..." << std::endl;
        std::string input;
        std::getline(std::cin, input);
    }

    std::cout << "Shutting down..." << std::endl;
    return 0;
}
