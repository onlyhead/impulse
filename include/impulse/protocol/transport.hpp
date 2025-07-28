#pragma once

#include "impulse/network/interface.hpp"
#include "impulse/protocol/message.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace impulse {

    template <typename MessageT = Message> class Transport {
      private:
        std::string name_;
        uint64_t join_time_;
        NetworkInterface *network_interface_;

        std::thread message_thread_;
        std::atomic<bool> running_;
        MessageT message_;

        bool continuous_;
        std::chrono::milliseconds interval_;

        // Generic message handler function
        std::function<void(const MessageT &, const std::string &, uint16_t)> message_handler_;

        inline void message_loop() {
            auto last_broadcast = std::chrono::steady_clock::now();
            while (running_) {
                if (continuous_) {
                    auto now = std::chrono::steady_clock::now();
                    message_.set_timestamp(now.time_since_epoch().count());
                    if (now - last_broadcast >= interval_) {
                        send_message(message_);
                        last_broadcast = now;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

      public:
        inline Transport(const std::string &name, NetworkInterface *network_interface)
            : name_(name), network_interface_(network_interface), running_(false), continuous_(false) {
            join_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
            running_ = true;
            message_thread_ = std::thread(&Transport<MessageT>::message_loop, this);
        }

        inline ~Transport() {
            running_ = false;
            if (message_thread_.joinable()) {
                message_thread_.join();
            }
        }

        inline void send_message(const MessageT &msg) {
            // Serialize and send via LAN interface
            auto size = msg.get_size();
            std::vector<char> buffer(size);
            msg.serialize(buffer.data());
            std::string message(buffer.data(), size);

            network_interface_->multicast_message(message);
        }

        inline std::string get_address() const { return network_interface_->get_address(); }

        // Set custom message handler
        inline void set_message_handler(std::function<void(const MessageT &, const std::string &, uint16_t)> handler) {
            message_handler_ = handler;
        }

        // Set message for continuous broadcasting
        inline void set_broadcast(const MessageT &message,
                                  std::chrono::milliseconds interval = std::chrono::milliseconds(1000)) {
            interval_ = interval;
            continuous_ = true;
            message_ = message;
        }

        inline void unset_broadcast() { continuous_ = false; }

        // Handle incoming message (for external routing)
        inline void handle_incoming_message(const std::string &message, const std::string &from_addr,
                                            uint16_t from_port) {
            MessageT msg;
            if (message.size() == msg.get_size()) {
                msg.deserialize(message.c_str());
                // Call custom handler if set
                if (message_handler_) {
                    message_handler_(msg, from_addr, from_port);
                }
            }
        }
    };

} // namespace impulse
