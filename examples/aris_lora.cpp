#include "impulse/network/interface.hpp"
#include "impulse/network/lan.hpp"
#include "impulse/network/lora.hpp"
#include "impulse/protocol/message.hpp"
#include "impulse/protocol/transport.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class Agent {
  private:
    std::string name_;
    std::string address_;
    impulse::Transport<impulse::Discovery> discovery_;
    impulse::Transport<impulse::Communication> communication_;
    impulse::Transport<impulse::Position> position_;
    impulse::Transport<impulse::Position> lora_position_;

  public:
    std::map<std::string, impulse::Discovery> all_discoveries_;
    std::map<std::string, impulse::Communication> all_communication_;
    std::map<std::string, impulse::Position> all_position_;

    inline Agent(const std::string &name, impulse::NetworkInterface *network_interface,
                 impulse::NetworkInterface *lora_interface, impulse::Discovery &discovery_msg,
                 impulse::Communication &communication_msg)
        : name_(name), address_(network_interface->get_address()), discovery_(name, network_interface),
          communication_(name, network_interface), position_(name, network_interface),
          lora_position_(name, lora_interface) {

        all_discoveries_[address_] = discovery_msg;
        discovery_.set_message_handler([this](const impulse::Discovery &msg, const std::string address,
                                              const uint16_t) { all_discoveries_[address] = msg; });
        discovery_.set_broadcast(discovery_msg);

        all_communication_[address_] = communication_msg;
        communication_.set_message_handler([this](const impulse::Communication &msg, const std::string address,
                                                  const uint16_t) { all_communication_[address] = msg; });
        communication_.set_broadcast(communication_msg);

        position_.set_message_handler([this](const impulse::Position &msg, const std::string address, const uint16_t) {
            all_position_[address] = msg;
        });

        network_interface->set_message_callback(
            [this](const std::string &message, const std::string &from_addr, uint16_t from_port) {
                discovery_.handle_incoming_message(message, from_addr, from_port);
                communication_.handle_incoming_message(message, from_addr, from_port);
                position_.handle_incoming_message(message, from_addr, from_port);
            });

        lora_position_.set_message_handler([this](const impulse::Position &msg, const std::string address,
                                                  const uint16_t) { all_position_[address] = msg; });
        lora_interface->set_message_callback(
            [this](const std::string &message, const std::string &from_addr, uint16_t /* from_port */) {
                lora_position_.handle_incoming_message(message, from_addr, 0);
            });
    }

    inline ~Agent() {}

    inline void update_position(const impulse::Position &position, impulse::NetworkInterface *lora_ptr) {
        all_position_[address_] = position;
        position_.send_message(position);
        if (lora_ptr->is_connected()) {
            lora_position_.send_message(position);
        }
    }
};

std::atomic<bool> should_exit{false};

void signal_handler(int signal) {
    std::cout << "\nCaught signal " << signal << ", requesting shutdown..." << std::endl;
    should_exit = true;
}

int main(int argc, char *argv[]) {
    int count = 0;
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <robot_name> [serial_port]" << std::endl;
        std::cerr << "Example: " << argv[0] << " Tractor-Alpha /dev/ttyUSB0" << std::endl;
        std::cerr << "Example: " << argv[0] << " Tractor-Alpha (LAN only)" << std::endl;
        return 1;
    }

    std::string robot_name = argv[1];
    std::string lora_port = (argc == 3) ? argv[2] : "";
    std::cout << "=== ARIS Robot: " << robot_name << " ===\n" << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    impulse::LanInterface lan("eno2");
    if (!lan.start()) {
        std::cerr << "Failed to start LAN interface" << std::endl;
        return 1;
    }

    impulse::LoRaInterface lora(lora_port, lan.get_address());
    if (!lora.start()) {
        std::cerr << "Failed to start LoRa interface" << std::endl;
        return 1;
    }

    auto now_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    impulse::Discovery self_msg = {};
    self_msg.timestamp = now_time;
    self_msg.join_time = now_time;
    self_msg.zero_ref = {40.7128, -74.0060, 0.0};
    self_msg.orchestrator = false;
    self_msg.capability_index = 64;

    impulse::Communication self_comm_msg = {};
    self_comm_msg.timestamp = now_time;
    self_comm_msg.transport_type = impulse::TransportType::dds;
    self_comm_msg.serialization_type = impulse::SerializationType::ros;

    Agent participant(robot_name, &lan, &lora, self_msg, self_comm_msg);

    impulse::Position position_msg = {};
    position_msg.timestamp = now_time;
    position_msg.pose.point = {40.7128, -74.0060, 0.0};
    participant.update_position(position_msg, &lora);

    auto start_time = std::chrono::steady_clock::now();
    while (!should_exit) {
        count++;
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::cout << "\n=== Current Network Status ===" << std::endl;
        for (const auto &[ipv6, agent] : participant.all_discoveries_) {
            std::cout << "    - " << ipv6 << ": " << agent.to_string() << std::endl;
        }
        std::cout << "\n=== Current Communication Status ===" << std::endl;
        for (const auto &[ipv6, agent] : participant.all_communication_) {
            std::cout << "    - " << ipv6 << ": " << agent.to_string() << std::endl;
        }
        std::cout << "\n=== Current Position Status ===" << std::endl;
        for (const auto &[ipv6, agent] : participant.all_position_) {
            std::cout << "    - " << ipv6 << ": " << agent.to_string() << std::endl;
        }

        position_msg.pose.point = {40.7128 + 0.1 * count, -74.0060 + 0.1 * count, 0.0};
        participant.update_position(position_msg, &lora);
    }

    std::cout << "Shutting down..." << std::endl;
    return 0;
}
