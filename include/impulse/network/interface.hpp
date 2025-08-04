#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace impulse {

    // Base class for transport objects
    class TransportBase {
      public:
        virtual ~TransportBase() = default;
        virtual void handle_incoming_message(const std::string &message, const std::string &from_addr,
                                             uint16_t from_port) = 0;
    };

    class NetworkInterface {
      public:
        virtual ~NetworkInterface() = default;

        // Pure virtual methods that all interfaces must implement
        virtual bool start() = 0;
        virtual void stop() = 0;

        virtual bool is_connected() const = 0;

        virtual void send_message(const std::string &dest_addr, uint16_t dest_port, const std::string &msg) = 0;
        virtual void multicast_message(const std::string &msg) = 0;
        virtual void multicast_to_group(const std::vector<std::string> &dest_addrs, uint16_t dest_port,
                                        const std::string &msg) = 0;

        virtual std::string get_address() const = 0;
        virtual uint16_t get_port() const = 0;
        virtual std::string get_interface_name() const = 0;

        virtual void
        set_message_callback(std::function<void(const std::string &, const std::string &, uint16_t)> callback) = 0;

        // Transport registration methods
        inline void add_transport(std::shared_ptr<TransportBase> transport);
        inline void remove_transport(std::shared_ptr<TransportBase> transport);

      protected:
        // Common fields that interfaces might use
        std::string address_;
        std::string interface_name_;
        uint16_t port_;
        std::function<void(const std::string &, const std::string &, uint16_t)> message_callback_;
        bool running_ = false;

        // List of registered transports
        std::vector<std::shared_ptr<TransportBase>> transports_;

        // Internal callback that routes to all registered transports
        inline void route_message_to_transports(const std::string &message, const std::string &from_addr, uint16_t from_port);
    };

} // namespace impulse

namespace impulse {

    // Inline implementations
    inline void NetworkInterface::add_transport(std::shared_ptr<TransportBase> transport) {
        transports_.push_back(transport);
        
        // Set up the callback to route messages to all transports if not already set
        if (!message_callback_) {
            set_message_callback([this](const std::string &message, const std::string &from_addr, uint16_t from_port) {
                route_message_to_transports(message, from_addr, from_port);
            });
        }
    }

    inline void NetworkInterface::remove_transport(std::shared_ptr<TransportBase> transport) {
        transports_.erase(std::remove(transports_.begin(), transports_.end(), transport), transports_.end());
    }

    inline void NetworkInterface::route_message_to_transports(const std::string &message, const std::string &from_addr,
                                                              uint16_t from_port) {
        for (auto &transport : transports_) {
            if (transport) {
                transport->handle_incoming_message(message, from_addr, from_port);
            }
        }
    }

} // namespace impulse
