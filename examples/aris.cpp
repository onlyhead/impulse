#include "impulse/agents.hpp"
#include "impulse/network/lan.hpp"

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

    Agent participant(robot_name, &lan, 75);

    std::cout << "Robot started. Waiting for discovery..." << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    while (!should_exit && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(60)) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::cout << "\n=== Current Network Status ===" << std::endl;
        participant.print_status();
        participant.send_discovery();
    }

    if (!should_exit) {
        std::cout << "\nDiscovery complete! Press Enter to shutdown..." << std::endl;
        std::string input;
        std::getline(std::cin, input);
    }

    std::cout << "Shutting down..." << std::endl;
    return 0;
}
