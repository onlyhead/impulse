#pragma once

#include "impulse/network/interface.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <random>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace impulse {

class LanInterface : public NetworkInterface {
  private:
    int socket_fd_;
    std::thread receive_thread_;
    bool owns_interface_;

    inline int create_tun_interface(const std::string &name) {
        int fd = open("/dev/net/tun", O_RDWR);
        if (fd < 0) {
            std::cerr << "Error opening /dev/net/tun: " << strerror(errno) << std::endl;
            return -1;
        }

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
        strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

        if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
            std::cerr << "Error creating TUN interface " << name << ": " << strerror(errno) << std::endl;
            close(fd);
            return -1;
        }

        if (ioctl(fd, TUNSETPERSIST, 1) < 0) {
            std::cerr << "Error making TUN persistent: " << strerror(errno) << std::endl;
            close(fd);
            return -1;
        }

        std::cout << "Created persistent TUN interface: " << ifr.ifr_name << std::endl;

        std::string cmd = "ip link set " + std::string(ifr.ifr_name) + " up";
        if (system(cmd.c_str()) != 0) {
            std::cerr << "Failed to bring interface up" << std::endl;
        }

        return fd;
    }

    inline std::string generate_robot_ipv6(int robot_id) {
        char ipv6_buf[INET6_ADDRSTRLEN];
        snprintf(ipv6_buf, sizeof(ipv6_buf), "fd00:dead:beef::%04x", robot_id);
        
        // Normalize the address using inet_pton/inet_ntop to ensure consistent format
        struct sockaddr_in6 sa;
        char normalized[INET6_ADDRSTRLEN];
        if (inet_pton(AF_INET6, ipv6_buf, &sa.sin6_addr) == 1) {
            if (inet_ntop(AF_INET6, &sa.sin6_addr, normalized, INET6_ADDRSTRLEN)) {
                return std::string(normalized);
            }
        }
        return std::string(ipv6_buf);
    }

    inline bool setup_interface() {
        if (owns_interface_) {
            int tun_fd = create_tun_interface(interface_name_);
            if (tun_fd < 0) {
                std::cerr << "Failed to create interface, falling back to loopback" << std::endl;
                interface_name_ = "lo";
                owns_interface_ = false;
            } else {
                close(tun_fd);
            }
        }

        std::string cmd = "ip -6 addr add " + address_ + "/64 dev " + interface_name_ + " 2>/dev/null";
        if (system(cmd.c_str()) != 0) {
            std::cerr << "Failed to add IPv6 address (try with sudo)" << std::endl;
        } else {
            std::cout << "Added IPv6 address " << address_ << " to " << interface_name_ << std::endl;
        }

        return true;
    }

    inline void receive_loop() {
        char buffer[1024];
        struct sockaddr_in6 from;
        socklen_t from_len;

        while (running_) {
            from_len = sizeof(from);
            ssize_t received =
                recvfrom(socket_fd_, buffer, sizeof(buffer), MSG_DONTWAIT, (struct sockaddr *)&from, &from_len);

            if (received > 0) {
                char addr_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &from.sin6_addr, addr_str, sizeof(addr_str));

                // Skip messages from ourselves
                if (std::string(addr_str) == address_) {
                    continue;
                }

                // If callback is set, call it with the binary data
                if (message_callback_) {
                    std::string message(buffer, received);
                    message_callback_(message, std::string(addr_str), ntohs(from.sin6_port));
                } else {
                    // Fallback to text printing for non-callback users
                    buffer[std::min((ssize_t)1023, received)] = '\0';
                    std::cout << address_ << " received: \"" << buffer << "\" from [" << addr_str
                              << "]:" << ntohs(from.sin6_port) << std::endl;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

  public:
    inline LanInterface(const std::string &interface = "", uint16_t port = 7447, const std::string &ipv6_addr = "")
        : socket_fd_(-1), owns_interface_(false) {

        port_ = port;

        if (interface.empty()) {
            interface_name_ = "robot_auto";
            owns_interface_ = true;
        } else {
            interface_name_ = interface;
            // Check if interface already exists
            owns_interface_ = (if_nametoindex(interface.c_str()) == 0);
        }

        if (ipv6_addr.empty()) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 65535);
            int robot_id = dis(gen);
            address_ = generate_robot_ipv6(robot_id);
        } else {
            address_ = ipv6_addr;
        }
    }

    inline ~LanInterface() { stop(); }

    inline bool start() override {
        if (!setup_interface()) {
            return false;
        }

        socket_fd_ = socket(AF_INET6, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << address_ << ": Failed to create socket" << std::endl;
            return false;
        }

        int v6only = 1;
        setsockopt(socket_fd_, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

        int reuse = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // Enable multicast loopback so we receive our own messages
        int loop = 1;
        setsockopt(socket_fd_, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop));

        struct sockaddr_in6 addr = {};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port_);

        if (inet_pton(AF_INET6, address_.c_str(), &addr.sin6_addr) == 1) {
            if (bind(socket_fd_, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                std::cout << address_ << " bound to [" << address_ << "]:" << port_ << std::endl;
            } else {
                addr.sin6_addr = in6addr_any;
                if (bind(socket_fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                    std::cerr << address_ << ": Failed to bind to port " << port_ << std::endl;
                    close(socket_fd_);
                    return false;
                }
                std::cout << address_ << " bound to [::] (any):" << port_ << std::endl;
            }
        }

        running_ = true;
        receive_thread_ = std::thread(&LanInterface::receive_loop, this);

        return true;
    }

    inline void stop() override {
        running_ = false;
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }

        if (owns_interface_) {
            std::string cmd = "ip link del " + interface_name_ + " 2>/dev/null";
            system(cmd.c_str());
        } else {
            std::string cmd = "ip -6 addr del " + address_ + "/64 dev " + interface_name_ + " 2>/dev/null";
            system(cmd.c_str());
        }
    }

    inline void send_message(const std::string &dest_addr, uint16_t dest_port, const std::string &msg) override {
        if (socket_fd_ < 0) return;

        // Create a separate socket bound to our specific IPv6 address
        int send_fd = socket(AF_INET6, SOCK_DGRAM, 0);
        if (send_fd < 0) return;

        int reuse = 1;
        setsockopt(send_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // Bind to our specific IPv6 address
        struct sockaddr_in6 src_addr = {};
        src_addr.sin6_family = AF_INET6;
        src_addr.sin6_port = htons(port_); // Use our port as source port
        inet_pton(AF_INET6, address_.c_str(), &src_addr.sin6_addr);

        if (bind(send_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
            close(send_fd);
            return;
        }

        struct sockaddr_in6 dest = {};
        dest.sin6_family = AF_INET6;
        dest.sin6_port = htons(dest_port);

        if (inet_pton(AF_INET6, dest_addr.c_str(), &dest.sin6_addr) != 1) {
            std::cerr << address_ << ": Invalid destination address" << std::endl;
            close(send_fd);
            return;
        }

        ssize_t sent = sendto(send_fd, msg.c_str(), msg.length(), 0, (struct sockaddr *)&dest, sizeof(dest));

        if (sent > 0) {
            std::cout << address_ << " sent: \"" << msg << "\" to [" << dest_addr << "]:" << dest_port << std::endl;
        }

        close(send_fd);
    }

    inline void multicast_message(const std::string &msg) override {
        if (socket_fd_ < 0) return;

        // Create a separate socket for multicast with our specific source address
        int mcast_fd = socket(AF_INET6, SOCK_DGRAM, 0);
        if (mcast_fd < 0) return;

        int reuse = 1;
        setsockopt(mcast_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // Enable multicast loopback so we receive our own messages
        int loop = 1;
        setsockopt(mcast_fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop));

        // Bind to our specific IPv6 address
        struct sockaddr_in6 src_addr = {};
        src_addr.sin6_family = AF_INET6;
        src_addr.sin6_port = htons(port_); // Use our port as source port
        inet_pton(AF_INET6, address_.c_str(), &src_addr.sin6_addr);

        if (bind(mcast_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
            close(mcast_fd);
            return;
        }

        // Use IPv6 multicast address ff02::1 (all nodes multicast)
        struct sockaddr_in6 dest = {};
        dest.sin6_family = AF_INET6;
        dest.sin6_port = htons(port_);

        if (inet_pton(AF_INET6, "ff02::1", &dest.sin6_addr) != 1) {
            std::cerr << address_ << ": Failed to set multicast address" << std::endl;
            close(mcast_fd);
            return;
        }

        ssize_t sent = sendto(mcast_fd, msg.c_str(), msg.length(), 0, (struct sockaddr *)&dest, sizeof(dest));

        if (sent > 0) {
        }

        close(mcast_fd);
    }

    inline void multicast_to_group(const std::vector<std::string> &dest_addrs, uint16_t dest_port,
                                   const std::string &msg) override {
        if (socket_fd_ < 0) return;

        // Create a separate socket bound to our specific IPv6 address
        int send_fd = socket(AF_INET6, SOCK_DGRAM, 0);
        if (send_fd < 0) return;

        int reuse = 1;
        setsockopt(send_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // Bind to our specific IPv6 address
        struct sockaddr_in6 src_addr = {};
        src_addr.sin6_family = AF_INET6;
        src_addr.sin6_port = htons(port_); // Use our port as source port
        inet_pton(AF_INET6, address_.c_str(), &src_addr.sin6_addr);

        if (bind(send_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
            close(send_fd);
            return;
        }

        std::cout << address_ << " multicasting: \"" << msg << "\" to group [";
        for (size_t i = 0; i < dest_addrs.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << dest_addrs[i];
        }
        std::cout << "]" << std::endl;

        for (const auto &dest_addr : dest_addrs) {
            struct sockaddr_in6 dest = {};
            dest.sin6_family = AF_INET6;
            dest.sin6_port = htons(dest_port);

            if (inet_pton(AF_INET6, dest_addr.c_str(), &dest.sin6_addr) != 1) {
                std::cerr << address_ << ": Invalid destination address " << dest_addr << std::endl;
                continue;
            }

            sendto(send_fd, msg.c_str(), msg.length(), 0, (struct sockaddr *)&dest, sizeof(dest));
        }

        close(send_fd);
    }

    inline std::string get_address() const override { return address_; }
    inline uint16_t get_port() const override { return port_; }
    inline std::string get_interface_name() const override { return interface_name_; }
    inline void
    set_message_callback(std::function<void(const std::string &, const std::string &, uint16_t)> callback) override {
        message_callback_ = callback;
    }

    // LAN-specific methods
    inline const std::string &get_ipv6() const { return address_; }
};

} // namespace impulse
