#include "impulse/protocol/aris.hpp"

int main() {
    std::cout << "=== ARIS P2P Discovery with LAN Demo ===" << std::endl;
    std::cout << "True peer-to-peer with automatic interface creation" << std::endl;
    std::cout << "Using IPv6 multicast group ff02::1234\n" << std::endl;

    ARISNetwork network;

    std::cout << "Adding Robot 1 (Tractor-Alpha)..." << std::endl;
    network.add_robot("Tractor-Alpha", 1001, 95);

    std::cout << "Waiting for Robot 1 to establish network..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(20));

    std::cout << "\nAdding Robot 2 (Harvester-Beta)..." << std::endl;
    network.add_robot("Harvester-Beta", 2002, 80);

    std::cout << "Waiting for Robot 2 to join network..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "\nAdding Robot 3 (Sprayer-Gamma)..." << std::endl;
    network.add_robot("Sprayer-Gamma", 3003, 60);

    std::cout << "Waiting for Robot 3 to join network..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "\nAdding Robot 4 (Feeder-Delta)..." << std::endl;
    network.add_robot("Feeder-Delta", 4004, 40);

    std::cout << "Waiting for Robot 4 to join network..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    network.print_network_status();

    std::cout << "\nP2P discovery complete! All robots are equal peers." << std::endl;
    std::cout << "Press Enter to shutdown..." << std::endl;
    std::cin.get();

    return 0;
}
