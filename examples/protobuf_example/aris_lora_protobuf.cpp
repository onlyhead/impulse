#include "impulse/network/interface.hpp"
#include "impulse/network/lan.hpp"
#include "impulse/network/lora.hpp"
#include "impulse/protocol/protobuf/protobuf.hpp"
#include "user_message.pb.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class ProtobufAgent {
  private:
    std::string name_;
    std::string address_;
    impulse::LoRaInterface* lora_interface_;

  public:
    std::map<std::string, user_messages::RobotPosition> all_positions_;
    std::map<std::string, user_messages::RobotDiscovery> all_discoveries_;

    inline ProtobufAgent(const std::string &name, impulse::NetworkInterface *network_interface,
                         impulse::LoRaInterface *lora_interface)
        : name_(name), address_(network_interface->get_address()), lora_interface_(lora_interface) {
        
        // Set up message callback for incoming LoRa messages
        lora_interface->set_message_callback(
            [this](const std::string &message, const std::string &from_addr, uint16_t /* from_port */) {
                handle_incoming_protobuf_message(message, from_addr);
            });
    }

    inline ~ProtobufAgent() {}

    inline void handle_incoming_protobuf_message(const std::string &message, const std::string &from_addr) {
        // Try to parse as RobotPosition first
        auto position_wrapper = impulse::protobuf::create<user_messages::RobotPosition>();
        if (position_wrapper->deserialize_from_network(message)) {
            all_positions_[from_addr] = position_wrapper->proto();
            std::cout << "Received position from " << from_addr << ": "
                      << "(" << position_wrapper->proto().x() << ", " 
                      << position_wrapper->proto().y() << ", " 
                      << position_wrapper->proto().z() << ")" << std::endl;
            return;
        }

        // Try to parse as RobotDiscovery
        auto discovery_wrapper = impulse::protobuf::create<user_messages::RobotDiscovery>();
        if (discovery_wrapper->deserialize_from_network(message)) {
            all_discoveries_[from_addr] = discovery_wrapper->proto();
            std::cout << "Received discovery from " << from_addr << ": "
                      << discovery_wrapper->proto().robot_name() << std::endl;
            return;
        }

        std::cout << "Failed to parse message from " << from_addr << std::endl;
    }

    inline void send_position(double x, double y, double z) {
        auto position_wrapper = impulse::protobuf::create<user_messages::RobotPosition>();
        position_wrapper->proto().set_robot_name(name_);
        position_wrapper->proto().set_x(x);
        position_wrapper->proto().set_y(y);
        position_wrapper->proto().set_z(z);
        position_wrapper->proto().set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        // Store our own position
        all_positions_[address_] = position_wrapper->proto();

        // Broadcast via LoRa
        std::string serialized = position_wrapper->serialize_for_network();
        lora_interface_->multicast_message(serialized);
    }

    inline void send_discovery() {
        auto discovery_wrapper = impulse::protobuf::create<user_messages::RobotDiscovery>();
        discovery_wrapper->proto().set_robot_name(name_);
        discovery_wrapper->proto().set_robot_type("Tractor");
        discovery_wrapper->proto().set_capabilities("positioning,communication");
        discovery_wrapper->proto().set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        // Store our own discovery
        all_discoveries_[address_] = discovery_wrapper->proto();

        // Broadcast via LoRa
        std::string serialized = discovery_wrapper->serialize_for_network();
        lora_interface_->multicast_message(serialized);
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
    std::cout << "=== ARIS Robot with Protobuf: " << robot_name << " ===\n" << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    impulse::LanInterface lan("eno1");
    if (!lan.start()) {
        std::cerr << "Failed to start LAN interface" << std::endl;
        return 1;
    }

    impulse::LoRaInterface lora(lora_port, lan.get_address());
    if (!lora.start()) {
        std::cerr << "Failed to start LoRa interface" << std::endl;
        return 1;
    }

    ProtobufAgent agent(robot_name, &lan, &lora);

    // Send initial discovery message
    agent.send_discovery();

    // Send initial position
    double base_x = 40.7128;
    double base_y = -74.0060;
    double base_z = 0.0;
    agent.send_position(base_x, base_y, base_z);

    while (!should_exit) {
        count++;
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::cout << "\n=== Current Protobuf Network Status ===" << std::endl;
        
        std::cout << "\n--- Discovered Robots ---" << std::endl;
        for (const auto &[address, discovery] : agent.all_discoveries_) {
            std::cout << "    - " << address << ": " << discovery.robot_name() 
                      << " (" << discovery.robot_type() << ") - " 
                      << discovery.capabilities() << std::endl;
        }
        
        std::cout << "\n--- Robot Positions ---" << std::endl;
        for (const auto &[address, position] : agent.all_positions_) {
            std::cout << "    - " << address << ": " << position.robot_name() 
                      << " at (" << position.x() << ", " << position.y() 
                      << ", " << position.z() << ")" << std::endl;
        }

        // Update our position (simulate movement)
        double new_x = base_x + 0.001 * count;  // Move slowly in longitude
        double new_y = base_y + 0.001 * count;  // Move slowly in latitude
        agent.send_position(new_x, new_y, base_z);

        // Occasionally resend discovery
        if (count % 3 == 0) {
            agent.send_discovery();
        }
    }

    std::cout << "Shutting down..." << std::endl;
    return 0;
}