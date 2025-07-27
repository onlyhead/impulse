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

int main() {
    std::cout << "=== ARIS P2P Discovery Demo ===" << std::endl;
    std::cout << "IPv6 multicast discovery on independent robots" << std::endl;
    std::cout << "Each robot runs independently and discovers others\n" << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Create LAN interfaces for each robot
    LanInterface lan1("eno2");
    LanInterface lan2("eno2");
    LanInterface lan3("eno2");
    LanInterface lan4("eno2");

    // Start the LAN interfaces
    if (!lan1.start()) {
        std::cerr << "Failed to start lan1" << std::endl;
        return 1;
    }
    if (!lan2.start()) {
        std::cerr << "Failed to start lan2" << std::endl;
        return 1;
    }
    if (!lan3.start()) {
        std::cerr << "Failed to start lan3" << std::endl;
        return 1;
    }
    if (!lan4.start()) {
        std::cerr << "Failed to start lan4" << std::endl;
        return 1;
    }

    // Create ARIS robots with their LAN interfaces
    Aris robot1("Tractor-Alpha", 1001, &lan1, 95);
    Aris robot2("Harvester-Beta", 2002, &lan2, 80);
    Aris robot3("Sprayer-Gamma", 3003, &lan3, 60);
    Aris robot4("Feeder-Delta", 4004, &lan4, 40);

    // Start ARIS discovery
    if (!robot1.start()) {
        std::cerr << "Failed to start robot1" << std::endl;
        return 1;
    }
    if (!robot2.start()) {
        std::cerr << "Failed to start robot2" << std::endl;
        return 1;
    }
    if (!robot3.start()) {
        std::cerr << "Failed to start robot3" << std::endl;
        return 1;
    }
    if (!robot4.start()) {
        std::cerr << "Failed to start robot4" << std::endl;
        return 1;
    }

    std::cout << "\nAll robots started. Waiting for discovery..." << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    while (!should_exit && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(30)) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::cout << "\n=== Current Network Status ===" << std::endl;
        robot1.print_status();
        robot2.print_status();
        robot3.print_status();
        robot4.print_status();
    }

    if (!should_exit) {
        std::cout << "\nP2P discovery complete! Press Enter to shutdown..." << std::endl;
        std::string input;
        std::getline(std::cin, input);
    }

    std::cout << "Shutting down..." << std::endl;
    return 0;
}
