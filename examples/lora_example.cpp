#include "impulse/network/lan.hpp"
#include "impulse/network/lora.hpp"
#include "impulse/protocol/transport.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace impulse;

int main() {
    std::cout << "=== Impulse Dual Interface Example (LAN + LoRa) ===" << std::endl;

    // Step 1: Create LAN interface first (always required)
    auto lan = std::make_unique<LanInterface>("eth0"); // Use actual network interface name

    if (!lan->start()) {
        std::cerr << "Failed to start LAN interface" << std::endl;
        return 1;
    }

    // Step 2: Get the IPv6 address from LAN interface (from DHCP/SLAAC or manual)
    std::string system_ipv6 = lan->get_address();
    std::cout << "System IPv6 address: " << system_ipv6 << std::endl;

    // Step 3: Create LoRa interface using SAME IPv6 address as LAN
    std::unique_ptr<LoRaInterface> lora;
    bool lora_available = false;

    try {
        lora = std::make_unique<LoRaInterface>("/dev/ttyUSB0", system_ipv6);

        // Set message callback for LoRa
        lora->set_message_callback([](const std::string &msg, const std::string &from, uint16_t /* port */) {
            std::cout << "LoRa received from " << from << ": " << msg << std::endl;
        });

        // Start LoRa interface (optional)
        if (lora->start()) {
            lora_available = true;
            std::cout << "LoRa interface available" << std::endl;
        } else {
            std::cout << "Warning: LoRa interface failed to start, continuing with LAN only" << std::endl;
            lora.reset();
        }
    } catch (const std::exception &e) {
        std::cout << "Warning: LoRa interface not available (" << e.what() << "), continuing with LAN only"
                  << std::endl;
        lora.reset();
    }

    // Step 4: Create transport layer using LAN interface (primary)
    // Note: Transport layer would need a concrete Message implementation
    // Transport<> transport("DualNode", lan.get());

    // Step 5: Set message callback for LAN
    lan->set_message_callback([](const std::string &msg, const std::string &from, uint16_t port) {
        std::cout << "LAN received from " << from << ":" << port << " - " << msg << std::endl;
    });

    // Step 6: Send test messages
    std::cout << "\n=== Sending Test Messages ===" << std::endl;

    // Send via LAN
    lan->send_message("2001:db8::200", 8080, "Hello via LAN!");

    // Send via LoRa (if available)
    if (lora_available && lora) {
        lora->send_message("2001:db8::200", 0, "Hello via LoRa mesh!");
        lora->multicast_message("Broadcast to LoRa mesh network");

        // Test LoRa-specific features
        std::cout << "\n=== LoRa Configuration ===" << std::endl;
        auto status = lora->get_status();
        std::cout << "LoRa Status:" << std::endl;
        std::cout << "  Current IPv6: " << status.current_ipv6 << std::endl;
        std::cout << "  Radio Active: " << (status.radio_active ? "Yes" : "No") << std::endl;
        std::cout << "  TX Power: " << (int)status.tx_power << std::endl;
        std::cout << "  Frequency: " << status.frequency_hz << " Hz" << std::endl;
        std::cout << "  Hop Limit: " << (int)status.hop_limit << std::endl;
        std::cout << "  Uptime: " << status.uptime_seconds << " seconds" << std::endl;

        // Test configuration changes
        std::cout << "\nTesting LoRa configuration changes..." << std::endl;
        lora->set_tx_power(20);
        lora->set_hop_limit(15);
    }

    // Step 7: Main loop - monitor both interfaces
    std::cout << "\n=== Running Dual Interface Node ===" << std::endl;
    std::cout << "Press Ctrl+C to exit..." << std::endl;

    int loop_count = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        loop_count++;

        std::cout << "\n--- Status Update " << loop_count << " ---" << std::endl;
        std::cout << "LAN: " << lan->get_address() << ":" << lan->get_port() << " (" << lan->get_interface_name() << ")"
                  << std::endl;

        if (lora_available && lora && lora->is_connected()) {
            auto status = lora->get_status();
            std::cout << "LoRa: " << status.current_ipv6 << " (active: " << (status.radio_active ? "Yes" : "No")
                      << ", uptime: " << status.uptime_seconds << "s)" << std::endl;
        } else {
            std::cout << "LoRa: Not available" << std::endl;
        }

        // Send periodic messages
        if (loop_count % 3 == 0) {
            std::string test_msg = "Periodic test " + std::to_string(loop_count);
            lan->multicast_message("LAN: " + test_msg);

            if (lora_available && lora) {
                lora->multicast_message("LoRa: " + test_msg);
            }
        }

        // Check for any pending LoRa messages
        if (lora_available && lora && lora->has_messages()) {
            auto messages = lora->get_pending_messages();
            std::cout << "Processing " << messages.size() << " pending LoRa messages" << std::endl;
        }
    }

    return 0;
}
