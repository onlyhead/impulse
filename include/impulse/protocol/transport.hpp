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
    int32_t capability_index_;
    uint64_t join_time_;
    NetworkInterface *network_interface_;

    std::thread message_thread_;
    std::atomic<bool> running_;

    // Generic message handler function
    std::function<void(const MessageT &, const std::string &)> message_handler_;

    void message_loop();
    void send_message(const MessageT &msg);
    void handle_incoming_message(const std::string &message, const std::string &from_addr);

  public:
    Transport(const std::string &name, NetworkInterface *network_interface, int32_t capability = 75);
    ~Transport();

    bool start();
    void stop();
    int32_t get_capability() const;
    std::string get_address() const;

    // Set custom message handler
    void set_message_handler(std::function<void(const MessageT &, const std::string &)> handler);

    // Send a message
    void send(const MessageT &message);
};

template <typename MessageT>
inline Transport<MessageT>::Transport(const std::string &name, NetworkInterface *network_interface, int32_t capability)
    : name_(name), capability_index_(capability), network_interface_(network_interface), running_(false) {

    join_time_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
}

template <typename MessageT> inline Transport<MessageT>::~Transport() { stop(); }

template <typename MessageT> inline bool Transport<MessageT>::start() {
    std::cout << name_ << " starting transport" << std::endl;

    // Register callback to receive messages from network interface
    network_interface_->set_message_callback(
        [this](const std::string &message, const std::string &from_addr, uint16_t from_port) {
            this->handle_incoming_message(message, from_addr);
        });

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
    while (running_) {
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
Transport<MessageT>::set_message_handler(std::function<void(const MessageT &, const std::string &)> handler) {
    message_handler_ = handler;
}

template <typename MessageT>
inline void Transport<MessageT>::handle_incoming_message(const std::string &message, const std::string &from_addr) {
    if (message.size() == sizeof(MessageT)) {
        MessageT msg;
        msg.deserialize(message.c_str());

        // Call custom handler if set
        if (message_handler_) {
            message_handler_(msg, from_addr);
        }
    }
}

template <typename MessageT> inline int32_t Transport<MessageT>::get_capability() const { return capability_index_; }

template <typename MessageT> inline std::string Transport<MessageT>::get_address() const {
    return network_interface_->get_address();
}
