#include "impulse/network/lora.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace impulse;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <serial_port> <ipv6_address>" << std::endl;
        std::cout << "Example: " << argv[0] << " /dev/ttyUSB0 2001:db8::42" << std::endl;
        return 1;
    }
    
    std::string serial_port = argv[1];
    std::string ipv6_addr = argv[2];
    
    std::cout << "=== Simple LoRa Interface Test ===" << std::endl;
    std::cout << "Serial Port: " << serial_port << std::endl;
    std::cout << "IPv6 Address: " << ipv6_addr << std::endl;
    
    try {
        // Create LoRa interface
        auto lora = std::make_unique<LoRaInterface>(serial_port, ipv6_addr);
        
        // Set message callback
        lora->set_message_callback([](const std::string& msg, const std::string& from, uint16_t /* port */) {
            std::cout << ">>> Received from " << from << ": " << msg << std::endl;
        });
        
        // Start interface
        if (!lora->start()) {
            std::cerr << "Failed to start LoRa interface" << std::endl;
            return 1;
        }
        
        std::cout << "LoRa interface started successfully!" << std::endl;
        
        // Get and display status
        auto status = lora->get_status();
        std::cout << "\nInitial Status:" << std::endl;
        std::cout << "  IPv6: " << status.current_ipv6 << std::endl;
        std::cout << "  Radio: " << (status.radio_active ? "Active" : "Inactive") << std::endl;
        std::cout << "  TX Power: " << (int)status.tx_power << std::endl;
        std::cout << "  Frequency: " << status.frequency_hz << " Hz" << std::endl;
        std::cout << "  Hop Limit: " << (int)status.hop_limit << std::endl;
        
        // Send test messages
        std::cout << "\n=== Sending Test Messages ===" << std::endl;
        
        // Test 1: Send to specific address
        lora->send_message("2001:db8::99", 0, "Hello specific node!");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Test 2: Broadcast message
        lora->multicast_message("Hello LoRa mesh network!");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Test 3: Group multicast
        std::vector<std::string> group = {"2001:db8::10", "2001:db8::20", "2001:db8::30"};
        lora->multicast_to_group(group, 0, "Group message test");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Test 4: Configuration changes
        std::cout << "\n=== Testing Configuration ===" << std::endl;
        lora->set_tx_power(15);
        lora->set_hop_limit(8);
        
        // Get updated status
        status = lora->get_status();
        std::cout << "Updated Status:" << std::endl;
        std::cout << "  TX Power: " << (int)status.tx_power << std::endl;
        std::cout << "  Hop Limit: " << (int)status.hop_limit << std::endl;
        
        // Main monitoring loop
        std::cout << "\n=== Monitoring (Press Ctrl+C to exit) ===" << std::endl;
        
        int count = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            count++;
            
            // Send periodic message
            std::string msg = "Periodic message #" + std::to_string(count);
            lora->multicast_message(msg);
            
            // Check connection status
            if (!lora->is_connected()) {
                std::cerr << "LoRa connection lost!" << std::endl;
                break;
            }
            
            // Display pending messages count
            if (lora->has_messages()) {
                auto messages = lora->get_pending_messages();
                std::cout << "Processed " << messages.size() << " pending messages" << std::endl;
                for (const auto& msg : messages) {
                    std::cout << "  - From " << msg.source_addr << ": " << msg.message 
                              << (msg.is_broadcast ? " [BROADCAST]" : "") << std::endl;
                }
            }
            
            // Get current status every 10 iterations
            if (count % 10 == 0) {
                status = lora->get_status();
                std::cout << "Status: IPv6=" << status.current_ipv6 
                          << ", Radio=" << (status.radio_active ? "OK" : "FAIL")
                          << ", Uptime=" << status.uptime_seconds << "s" << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}