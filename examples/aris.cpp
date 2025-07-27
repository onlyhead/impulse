#include "impulse/protocol/aris.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

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

    // Generate a simple robot ID from the robot name hash
    std::hash<std::string> hasher;
    uint32_t robot_id = static_cast<uint32_t>(hasher(robot_name) % 9000 + 1000);

    // Set capability based on robot name
    int32_t capability = 75; // default
    if (robot_name.find("Tractor") != std::string::npos)
        capability = 95;
    else if (robot_name.find("Harvester") != std::string::npos)
        capability = 80;
    else if (robot_name.find("Sprayer") != std::string::npos)
        capability = 60;
    else if (robot_name.find("Feeder") != std::string::npos)
        capability = 40;

    Aris participant(robot_name, robot_id, &lan, capability);
    if (!participant.start()) {
        std::cerr << "Failed to start robot" << std::endl;
        return 1;
    }

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
