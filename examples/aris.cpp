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
    Transport<Discovery> discovery_;
    Transport<Communication> communication_;

  public:
    std::map<std::string, Discovery> all_discoveries_;
    std::map<std::string, Communication> all_communication_;

    inline Agent(const std::string &name, NetworkInterface *network_interface, Discovery &discovery_msg,
                 Communication &communication_msg)
        : name_(name), discovery_(name, network_interface, true, std::chrono::milliseconds(1000)),
          communication_(name, network_interface, true, std::chrono::milliseconds(1000)) {

        all_discoveries_[discovery_msg.ipv6] = discovery_msg;
        discovery_.set_message_handler([this](const Discovery &msg, const std::string, const uint16_t) {
            std::string agent_ipv6(msg.ipv6);
            all_discoveries_[agent_ipv6] = msg;
        });
        discovery_.set_broadcast_message(discovery_msg);
        discovery_.start();

        all_communication_[communication_msg.ipv6] = communication_msg;
        communication_.set_message_handler([this](const Communication &msg, const std::string, const uint16_t) {
            std::string agent_ipv6(msg.ipv6);
            all_communication_[agent_ipv6] = msg;
        });
        communication_.set_broadcast_message(communication_msg);
        communication_.start();
    }

    inline ~Agent() { discovery_.stop(); }
};

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
    auto now_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    Discovery self_msg = {};
    strncpy(self_msg.ipv6, lan.get_address().c_str(), 45);
    self_msg.timestamp = now_time;
    self_msg.join_time = now_time; // Set join_time once at startup
    self_msg.orchestrator = false;
    self_msg.zero_ref = {40.7128, -74.0060, 0.0};
    self_msg.capability_index = 64;

    Communication self_comm_msg = {};
    self_comm_msg.timestamp = now_time;
    strncpy(self_comm_msg.ipv6, lan.get_address().c_str(), 45);
    self_comm_msg.transport_type = TransportType::dds;
    self_comm_msg.serialization_type = SerializationType::ros;

    Agent participant(robot_name, &lan, self_msg, self_comm_msg);

    std::cout << "Robot started. Waiting for discovery..." << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    while (!should_exit && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(60)) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::cout << "\n=== Current Network Status ===" << std::endl;
        for (const auto &[ipv6, agent] : participant.all_discoveries_) {
            std::cout << "    - " << agent.to_string() << std::endl;
        }
        std::cout << "\n=== Current Communication Status ===" << std::endl;
        for (const auto &[ipv6, agent] : participant.all_communication_) {
            std::cout << "    - " << agent.to_string() << std::endl;
        }
    }

    if (!should_exit) {
        std::cout << "\nDiscovery complete! Press Enter to shutdown..." << std::endl;
        std::string input;
        std::getline(std::cin, input);
    }

    std::cout << "Shutting down..." << std::endl;
    return 0;
}
