#ifdef ENABLE_PROTOBUF

#include "impulse/protocol/protobuf/protobuf.hpp"
#include "build/examples/protobuf_example/user_message.pb.h"

#include <iostream>

using namespace impulse;

int main() {
    std::cout << "Testing minimal protobuf functionality..." << std::endl;
    
    try {
        // Create a simple protobuf wrapper
        auto wrapper = protobuf::create<user::RobotStatus>();
        
        // Set some basic fields
        wrapper->proto().set_robot_id("test_robot");
        wrapper->proto().set_battery_level(75.5);
        wrapper->proto().set_state(user::IDLE);
        
        std::cout << "Created protobuf message:" << std::endl;
        std::cout << wrapper->proto().robot_id() << std::endl;
        std::cout << "Battery: " << wrapper->proto().battery_level() << "%" << std::endl;
        
        // Test serialization
        std::string network_data = wrapper->serialize_for_network();
        std::cout << "Serialized size: " << network_data.size() << " bytes" << std::endl;
        
        // Test deserialization
        auto received = ProtobufWrapper<user::RobotStatus>::from_network_data(network_data);
        if (received) {
            std::cout << "Deserialized successfully!" << std::endl;
            std::cout << "Robot ID: " << received->proto().robot_id() << std::endl;
            std::cout << "Battery: " << received->proto().battery_level() << "%" << std::endl;
        } else {
            std::cout << "Deserialization failed!" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}

#else

#include <iostream>

int main() {
    std::cout << "Protobuf not enabled" << std::endl;
    return 1;
}

#endif