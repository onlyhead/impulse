/**
 * @file protobuf_user_example.cpp
 * @brief Example showing how users can use their own .proto files with impulse
 * 
 * This demonstrates the proper way to integrate custom protobuf messages:
 * 1. User creates their own .proto file (user_message.proto)
 * 2. User generates C++ code: protoc --cpp_out=. user_message.proto
 * 3. User includes the generated header and uses ProtobufWrapper
 * 4. Library handles all network serialization automatically
 */

#ifdef ENABLE_PROTOBUF

#include "impulse/protocol/protobuf/protobuf.hpp"
#include "impulse/network/lan.hpp"
#include "impulse/network/lora.hpp"

// User includes their own generated protobuf headers
#include "user_message.pb.h"

#include <iostream>
#include <chrono>
#include <thread>

using namespace impulse;

void demonstrate_custom_robot_status() {
    std::cout << "=== Custom Robot Status Message ===" << std::endl;
    
    // Create wrapper for user's custom message
    auto robot_status = protobuf::create<user_messages::RobotStatus>();
    
    // User fills their own protobuf message normally
    robot_status->proto().set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    robot_status->proto().set_robot_id("robot_007");
    robot_status->proto().set_battery_level(85.5);
    robot_status->proto().set_state(user_messages::MOVING);
    
    // Set position
    auto* pos = robot_status->proto().mutable_position();
    pos->set_x(10.5);
    pos->set_y(20.3);
    pos->set_z(1.2);
    pos->set_heading(1.57); // 90 degrees
    
    // Add missions
    robot_status->proto().add_active_missions("patrol_zone_A");
    robot_status->proto().add_active_missions("collect_samples");
    
    std::cout << "Created robot status message:" << std::endl;
    std::cout << robot_status->to_string() << std::endl;
    
    // Serialize for network (automatically adds size prefix)
    std::string network_data = robot_status->serialize_for_network();
    std::cout << "Serialized size: " << network_data.size() << " bytes" << std::endl;
    
    // Send via LAN
    LanInterface lan("lo", 7447);
    if (lan.start()) {
        lan.multicast_message(network_data);
        std::cout << "Sent via LAN multicast" << std::endl;
        lan.stop();
    }
}

void demonstrate_custom_sensor_data() {
    std::cout << "\n=== Custom Sensor Data Message ===" << std::endl;
    
    // Create wrapper for sensor data
    auto sensor_msg = protobuf::create<user_messages::SensorData>();
    
    sensor_msg->proto().set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    sensor_msg->proto().set_sensor_id("temp_sensor_01");
    
    // Set temperature data
    auto* temp = sensor_msg->proto().mutable_temperature();
    temp->set_celsius(23.5);
    temp->set_humidity(65.2);
    
    std::cout << "Created sensor data message:" << std::endl;
    std::cout << sensor_msg->to_string() << std::endl;
    
    // Serialize for LoRa (small payload)
    std::string network_data = sensor_msg->serialize_for_network();
    std::cout << "Serialized size: " << network_data.size() << " bytes" << std::endl;
    
    // LoRa example (commented out - requires hardware)
    /*
    LoRaInterface lora("/dev/ttyUSB0", "fd00:dead:beef::1");
    if (lora.start()) {
        lora.send_message("fd00:dead:beef::2", 0, network_data);
        std::cout << "Sent via LoRa to specific node" << std::endl;
        lora.stop();
    }
    */
}

void demonstrate_message_receiving() {
    std::cout << "\n=== Receiving Custom Messages ===" << std::endl;
    
    // Simulate receiving a message over the network
    auto original_msg = protobuf::create<user_messages::RobotStatus>();
    original_msg->proto().set_robot_id("robot_123");
    original_msg->proto().set_battery_level(42.0);
    original_msg->proto().set_state(user_messages::CHARGING);
    
    // Serialize as if received from network
    std::string network_data = original_msg->serialize_for_network();
    
    // Deserialize received message
    auto received_msg = ProtobufWrapper<user_messages::RobotStatus>::from_network_data(network_data);
    
    if (received_msg) {
        std::cout << "Successfully received and deserialized message:" << std::endl;
        std::cout << "Robot ID: " << received_msg->proto().robot_id() << std::endl;
        std::cout << "Battery: " << received_msg->proto().battery_level() << "%" << std::endl;
        std::cout << "State: " << received_msg->proto().state() << std::endl;
    } else {
        std::cout << "Failed to deserialize message" << std::endl;
    }
}

int main() {
    std::cout << "Impulse Protocol Buffers - User Example" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Protobuf available: " << (protobuf::is_available() ? "Yes" : "No") << std::endl;
    std::cout << "Version: " << protobuf::get_version() << std::endl;
    std::cout << std::endl;
    
    if (!protobuf::is_available()) {
        std::cout << "Protobuf support not compiled. Build with -DENABLE_PROTOBUF=ON" << std::endl;
        return 1;
    }
    
    try {
        demonstrate_custom_robot_status();
        demonstrate_custom_sensor_data();
        demonstrate_message_receiving();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}

#else

#include <iostream>

int main() {
    std::cout << "Protobuf support not compiled. Build with -DENABLE_PROTOBUF=ON" << std::endl;
    std::cout << "To enable protobuf:" << std::endl;
    std::cout << "  1. Install protobuf development libraries" << std::endl;
    std::cout << "  2. Configure with: cmake -DENABLE_PROTOBUF=ON" << std::endl;
    std::cout << "  3. Rebuild the project" << std::endl;
    return 1;
}

#endif // ENABLE_PROTOBUF