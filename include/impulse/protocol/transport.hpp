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

template <typename MessageT = Message> class Transport {
  private:
    std::string name_;
    uint64_t join_time_;
    NetworkInterface *network_interface_;

    std::thread message_thread_;
    std::atomic<bool> running_;
    bool continuous_;
    std::chrono::milliseconds broadcast_interval_;
    MessageT broadcast_message_;
    bool has_broadcast_message_;

    // Generic message handler function
    std::function<void(const MessageT &, const std::string &, uint16_t)> message_handler_;

    void message_loop();
    void send_message(const MessageT &msg);

  public:
    Transport(const std::string &name, NetworkInterface *network_interface, bool continuous = false,
              std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    ~Transport();

    bool start();
    void stop();
    std::string get_address() const;

    // Set custom message handler
    void set_message_handler(std::function<void(const MessageT &, const std::string &, uint16_t)> handler);

    // Send a message
    void send(const MessageT &message);

    // Set message for continuous broadcasting
    void set_broadcast_message(const MessageT &message);

    // Handle incoming message (for external routing)
    void handle_incoming_message(const std::string &message, const std::string &from_addr, uint16_t from_port);
};

template <typename MessageT>
inline Transport<MessageT>::Transport(const std::string &name, NetworkInterface *network_interface, bool continuous,
                                      std::chrono::milliseconds interval)
    : name_(name), network_interface_(network_interface), running_(false), continuous_(continuous),
      broadcast_interval_(interval), has_broadcast_message_(false) {

    join_time_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
}

template <typename MessageT> inline Transport<MessageT>::~Transport() { stop(); }

template <typename MessageT> inline bool Transport<MessageT>::start() {
    std::cout << name_ << " starting transport" << std::endl;

    running_ = true;
    message_thread_ = std::thread(&Transport<MessageT>::message_loop, this);

    return true;
}

template <typename MessageT> inline void Transport<MessageT>::stop() {
    running_ = false;
    if (message_thread_.joinable()) {
        message_thread_.join();
    }
}

template <typename MessageT> inline void Transport<MessageT>::message_loop() {
    auto last_broadcast = std::chrono::steady_clock::now();

    while (running_) {
        if (continuous_ && has_broadcast_message_) {
            auto now = std::chrono::steady_clock::now();
            broadcast_message_.set_timestamp(now.time_since_epoch().count());
            if (now - last_broadcast >= broadcast_interval_) {
                send_message(broadcast_message_);
                last_broadcast = now;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

template <typename MessageT> inline void Transport<MessageT>::send_message(const MessageT &msg) {
    // Serialize and send via LAN interface
    auto size = msg.get_size();
    std::vector<char> buffer(size);
    msg.serialize(buffer.data());
    std::string message(buffer.data(), size);

    network_interface_->multicast_message(message);
}

template <typename MessageT> inline void Transport<MessageT>::send(const MessageT &message) { send_message(message); }

template <typename MessageT>
inline void
Transport<MessageT>::set_message_handler(std::function<void(const MessageT &, const std::string &, uint16_t)> handler) {
    message_handler_ = handler;
}

template <typename MessageT>
inline void Transport<MessageT>::handle_incoming_message(const std::string &message, const std::string &from_addr, uint16_t from_port) {
    MessageT msg;
    if (message.size() == msg.get_size()) {
        msg.deserialize(message.c_str());

        // Call custom handler if set
        if (message_handler_) {
            message_handler_(msg, from_addr, from_port);
        }
    }
}

template <typename MessageT> inline std::string Transport<MessageT>::get_address() const {
    return network_interface_->get_address();
}

template <typename MessageT> inline void Transport<MessageT>::set_broadcast_message(const MessageT &message) {
    broadcast_message_ = message;
    has_broadcast_message_ = true;
}
