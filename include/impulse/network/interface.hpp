#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace impulse {

    class NetworkInterface {
      public:
        virtual ~NetworkInterface() = default;

        // Pure virtual methods that all interfaces must implement
        virtual bool start() = 0;
        virtual void stop() = 0;

        virtual void send_message(const std::string &dest_addr, uint16_t dest_port, const std::string &msg) = 0;
        virtual void multicast_message(const std::string &msg) = 0;
        virtual void multicast_to_group(const std::vector<std::string> &dest_addrs, uint16_t dest_port,
                                        const std::string &msg) = 0;

        virtual std::string get_address() const = 0;
        virtual uint16_t get_port() const = 0;
        virtual std::string get_interface_name() const = 0;

        virtual void
        set_message_callback(std::function<void(const std::string &, const std::string &, uint16_t)> callback) = 0;

      protected:
        // Common fields that interfaces might use
        std::string address_;
        std::string interface_name_;
        uint16_t port_;
        std::function<void(const std::string &, const std::string &, uint16_t)> message_callback_;
        bool running_ = false;
    };

} // namespace impulse
