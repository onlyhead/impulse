#pragma once

#include "impulse/network/interface.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace impulse {

    // Protocol constants matching melodi firmware
    enum SerialCommand : uint8_t {
        CMD_SEND_MESSAGE = 0x01,
        CMD_SET_IPV6 = 0x02,
        CMD_GET_STATUS = 0x03,
        CMD_SET_CONFIG = 0x04,
        CMD_RESET_NODE = 0x05,
        CMD_GET_NEIGHBORS = 0x06
    };

    enum ResponseType : uint8_t {
        RESP_ACK = 0x80,
        RESP_NACK = 0x81,
        RESP_STATUS = 0x82,
        RESP_MESSAGE = 0x83,
        RESP_ERROR = 0x84
    };

    enum ErrorCode : uint8_t {
        ERR_INVALID_COMMAND = 0x01,
        ERR_INVALID_IPV6 = 0x02,
        ERR_RADIO_FAILURE = 0x03,
        ERR_BUFFER_OVERFLOW = 0x04,
        ERR_TIMEOUT = 0x05,
        ERR_CHECKSUM_FAILED = 0x06
    };

    struct LoRaStatus {
        std::string current_ipv6;
        bool radio_active;
        uint8_t tx_power;
        uint32_t frequency_hz;
        uint8_t hop_limit;
        uint16_t uptime_seconds;
    };

    struct IncomingMessage {
        std::string source_addr;
        std::string message;
        bool is_broadcast;
        std::chrono::steady_clock::time_point received_time;
    };

    class LoRaInterface : public NetworkInterface {
      private:
        // Serial communication
        std::string serial_port_;
        int serial_fd_;
        bool serial_connected_;

        // Threading
        std::thread listen_thread_;
        std::thread heartbeat_thread_;
        std::atomic<bool> running_;

        // Message handling
        std::queue<IncomingMessage> incoming_messages_;
        mutable std::mutex message_queue_mutex_;
        std::condition_variable message_available_;

        // Command synchronization
        mutable std::mutex command_mutex_;
        std::condition_variable command_response_;
        std::map<uint8_t, std::vector<uint8_t>> pending_responses_;
        std::chrono::milliseconds command_timeout_;

        // Node state
        std::string node_ipv6_;
        LoRaStatus current_status_;
        mutable std::mutex status_mutex_;

        // Broadcast address constant
        static constexpr const char *BROADCAST_IPV6 = "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff";

        // Address conversion helpers
        inline std::vector<uint8_t> string_to_ipv6_bytes(const std::string &addr) {
            std::vector<uint8_t> bytes(16, 0);

            struct sockaddr_in6 sa;
            int result = inet_pton(AF_INET6, addr.c_str(), &(sa.sin6_addr));

            if (result == 1) {
                memcpy(bytes.data(), &sa.sin6_addr, 16);
            }

            return bytes;
        }

        inline std::string ipv6_bytes_to_string(const std::vector<uint8_t> &bytes) {
            if (bytes.size() != 16) {
                return "";
            }

            char str[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, bytes.data(), str, INET6_ADDRSTRLEN) != nullptr) {
                return std::string(str);
            }

            return "";
        }

        inline bool is_valid_ipv6(const std::string &addr) {
            struct sockaddr_in6 sa;
            return inet_pton(AF_INET6, addr.c_str(), &(sa.sin6_addr)) == 1;
        }

        // Serial operations
        inline bool open_serial_port() {
            serial_fd_ = open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
            if (serial_fd_ == -1) {
                return false;
            }

            // Configure serial port
            struct termios options;
            tcgetattr(serial_fd_, &options);

            // Set baud rate to 115200
            cfsetispeed(&options, B115200);
            cfsetospeed(&options, B115200);

            // 8N1
            options.c_cflag &= ~PARENB; // No parity
            options.c_cflag &= ~CSTOPB; // 1 stop bit
            options.c_cflag &= ~CSIZE;  // Clear data size bits
            options.c_cflag |= CS8;     // 8 data bits

            // Enable receiver and set local mode
            options.c_cflag |= (CLOCAL | CREAD);

            // Raw input
            options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

            // Raw output
            options.c_oflag &= ~OPOST;

            // No flow control
            options.c_iflag &= ~(IXON | IXOFF | IXANY);

            // Apply settings
            tcsetattr(serial_fd_, TCSANOW, &options);

            // Flush buffers
            tcflush(serial_fd_, TCIOFLUSH);

            serial_connected_ = true;
            return true;
        }

        inline void close_serial_port() {
            if (serial_fd_ != -1) {
                close(serial_fd_);
                serial_fd_ = -1;
            }
            serial_connected_ = false;
        }

        inline bool write_serial(const std::vector<uint8_t> &data) {
            if (serial_fd_ == -1 || data.empty()) {
                return false;
            }

            ssize_t bytes_written = write(serial_fd_, data.data(), data.size());
            return bytes_written == static_cast<ssize_t>(data.size());
        }

        inline std::vector<uint8_t> read_serial(size_t max_bytes = 1024) {
            std::vector<uint8_t> data;

            if (serial_fd_ == -1) {
                return data;
            }

            std::vector<uint8_t> buffer(max_bytes);
            ssize_t bytes_read = read(serial_fd_, buffer.data(), max_bytes);

            if (bytes_read > 0) {
                data.assign(buffer.begin(), buffer.begin() + bytes_read);
            }

            return data;
        }

        // Protocol handling
        inline bool send_command(SerialCommand cmd, const std::vector<uint8_t> &data = {}) {
            if (!serial_connected_) {
                return false;
            }

            std::lock_guard<std::mutex> lock(command_mutex_);

            // Prepare command packet
            std::vector<uint8_t> packet;
            packet.push_back(static_cast<uint8_t>(cmd));
            packet.insert(packet.end(), data.begin(), data.end());

            return write_serial(packet);
        }

        inline bool wait_for_response(SerialCommand cmd, std::vector<uint8_t> &response,
                                      std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
            std::unique_lock<std::mutex> lock(command_mutex_);

            auto wait_result = command_response_.wait_for(lock, timeout, [this, cmd] {
                return pending_responses_.find(static_cast<uint8_t>(cmd)) != pending_responses_.end();
            });

            if (wait_result) {
                response = pending_responses_[static_cast<uint8_t>(cmd)];
                pending_responses_.erase(static_cast<uint8_t>(cmd));
                return true;
            }

            return false;
        }

        inline void parse_response(const std::vector<uint8_t> &packet) {
            if (packet.size() < 5) {
                return;
            }

            ResponseType response_type = static_cast<ResponseType>(packet[4]);
            std::vector<uint8_t> data(packet.begin() + 5, packet.end());

            switch (response_type) {
            case RESP_MESSAGE: {
                if (data.size() >= 19) { // flag + src + len
                    bool is_broadcast = data[0] != 0;
                    std::vector<uint8_t> src_bytes(data.begin() + 1, data.begin() + 17);
                    std::string src_addr = ipv6_bytes_to_string(src_bytes);
                    uint16_t msg_len = (data[17] << 8) | data[18];

                    if (data.size() >= 19 + msg_len) {
                        std::string message(data.begin() + 19, data.begin() + 19 + msg_len);

                        // Add to message queue
                        {
                            std::lock_guard<std::mutex> lock(message_queue_mutex_);
                            incoming_messages_.push(
                                {src_addr, message, is_broadcast, std::chrono::steady_clock::now()});
                        }
                        message_available_.notify_one();

                        // Call callback if set
                        if (message_callback_) {
                            message_callback_(message, src_addr, 0);
                        }
                    }
                }
                break;
            }

            case RESP_ACK:
            case RESP_NACK:
            case RESP_STATUS:
            case RESP_ERROR: {
                // Store response for waiting command
                std::lock_guard<std::mutex> lock(command_mutex_);
                if (!data.empty()) {
                    uint8_t original_cmd = data[0];
                    pending_responses_[original_cmd] = data;
                }
                command_response_.notify_all();
                break;
            }
            }
        }

        // Background threads
        inline void listen_thread_func() {
            std::vector<uint8_t> buffer;
            const std::vector<uint8_t> HEADER = {0xAA, 0xBB, 0xCC, 0xDD};

            while (running_) {
                auto data = read_serial(1024);
                if (!data.empty()) {
                    buffer.insert(buffer.end(), data.begin(), data.end());

                    // Look for complete packets starting with header
                    while (buffer.size() >= 5) { // Header + response type
                        auto header_pos = std::search(buffer.begin(), buffer.end(), HEADER.begin(), HEADER.end());

                        if (header_pos == buffer.end()) {
                            // No header found, clear buffer
                            buffer.clear();
                            break;
                        }

                        // Remove data before header
                        if (header_pos != buffer.begin()) {
                            buffer.erase(buffer.begin(), header_pos);
                        }

                        if (buffer.size() < 5) {
                            break; // Need more data
                        }

                        ResponseType response_type = static_cast<ResponseType>(buffer[4]);

                        // Determine expected packet length based on response type
                        size_t expected_length = 5; // Header + response type

                        switch (response_type) {
                        case RESP_ACK:
                            expected_length += 1; // Original command
                            break;
                        case RESP_NACK:
                            expected_length += 2; // Original command + error code
                            break;
                        case RESP_STATUS:
                            expected_length += 25; // Status data
                            break;
                        case RESP_MESSAGE:
                            if (buffer.size() >= 5 + 1 + 16 + 2) { // Need length bytes
                                uint16_t msg_len = (buffer[5 + 1 + 16] << 8) | buffer[5 + 1 + 16 + 1];
                                expected_length += 1 + 16 + 2 + msg_len; // flag + src + len + payload
                            } else {
                                break; // Need more data
                            }
                            break;
                        case RESP_ERROR:
                            expected_length += 1; // Error code (minimum)
                            break;
                        }

                        if (buffer.size() >= expected_length) {
                            // Extract complete packet
                            std::vector<uint8_t> packet(buffer.begin(), buffer.begin() + expected_length);
                            buffer.erase(buffer.begin(), buffer.begin() + expected_length);

                            // Process the packet
                            parse_response(packet);
                        } else {
                            break; // Need more data
                        }
                    }
                } else {
                    // No data available, small delay to prevent busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }

        inline void heartbeat_thread_func() {
            auto last_status_check = std::chrono::steady_clock::now();
            const auto status_interval = std::chrono::seconds(30);

            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                auto now = std::chrono::steady_clock::now();
                if (now - last_status_check >= status_interval) {
                    // Periodic status check to ensure connection is alive
                    get_status();
                    last_status_check = now;
                }
            }
        }

      public:
        inline explicit LoRaInterface(const std::string &serial_port, const std::string &node_ipv6)
            : serial_port_(serial_port), serial_fd_(-1), serial_connected_(false), running_(false),
              command_timeout_(std::chrono::milliseconds(5000)), node_ipv6_(node_ipv6) {

            interface_name_ = "LoRa-" + serial_port;

            // IPv6 address must be provided - same as LAN interface
            // This should match the system's IPv6 from LAN interface
            if (node_ipv6_.empty()) {
                std::cerr << "ERROR: LoRa interface requires IPv6 address (same as LAN interface)" << std::endl;
                throw std::runtime_error("LoRa interface requires IPv6 address");
            }
        }

        inline virtual ~LoRaInterface() { stop(); }

        // NetworkInterface implementation
        inline bool start() override {
            if (running_) {
                return true;
            }

            // Open serial connection
            if (!open_serial_port()) {
                std::cerr << "Failed to open serial port: " << serial_port_ << std::endl;
                return false;
            }

            running_ = true;

            // Start background threads
            listen_thread_ = std::thread(&LoRaInterface::listen_thread_func, this);
            heartbeat_thread_ = std::thread(&LoRaInterface::heartbeat_thread_func, this);

            // Wait a moment for connection to stabilize
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Set the node's IPv6 address (must match LAN interface)
            if (!set_node_ipv6(node_ipv6_)) {
                std::cerr << "Failed to set IPv6 address on LoRa node: " << node_ipv6_ << std::endl;
                stop();
                return false;
            }

            // Get initial status
            current_status_ = get_status();

            std::cout << "LoRa interface started on " << serial_port_ << " with IPv6: " << get_address() << std::endl;

            return true;
        }

        inline void stop() override {
            if (!running_) {
                return;
            }

            running_ = false;

            // Wake up any waiting threads
            message_available_.notify_all();
            command_response_.notify_all();

            // Join threads
            if (listen_thread_.joinable()) {
                listen_thread_.join();
            }
            if (heartbeat_thread_.joinable()) {
                heartbeat_thread_.join();
            }

            close_serial_port();
            std::cout << "LoRa interface stopped" << std::endl;
        }

        inline void send_message(const std::string &dest_addr, uint16_t /* dest_port */,
                                 const std::string &msg) override {
            if (!running_ || !serial_connected_) {
                std::cerr << "LoRa interface not connected" << std::endl;
                return;
            }

            // Validate destination address
            if (!is_valid_ipv6(dest_addr)) {
                std::cerr << "Invalid IPv6 address: " << dest_addr << std::endl;
                return;
            }

            // Convert destination to bytes
            auto dest_bytes = string_to_ipv6_bytes(dest_addr);
            if (dest_bytes.size() != 16) {
                std::cerr << "IPv6 address conversion failed" << std::endl;
                return;
            }

            // Prepare command data: [2 bytes: length][16 bytes: dest][N bytes: payload]
            std::vector<uint8_t> command_data;

            // Payload length (big-endian)
            uint16_t payload_len = msg.length();
            command_data.push_back((payload_len >> 8) & 0xFF);
            command_data.push_back(payload_len & 0xFF);

            // Destination IPv6
            command_data.insert(command_data.end(), dest_bytes.begin(), dest_bytes.end());

            // Message payload
            command_data.insert(command_data.end(), msg.begin(), msg.end());

            // Send command
            if (send_command(CMD_SEND_MESSAGE, command_data)) {
                std::cout << "LoRa message sent to " << dest_addr << ": " << msg.substr(0, 50)
                          << (msg.length() > 50 ? "..." : "") << std::endl;
            } else {
                std::cerr << "Failed to send LoRa message to " << dest_addr << std::endl;
            }
        }

        inline void multicast_message(const std::string &msg) override { send_message(BROADCAST_IPV6, 0, msg); }

        inline void multicast_to_group(const std::vector<std::string> &dest_addrs, uint16_t dest_port,
                                       const std::string &msg) override {
            for (const auto &addr : dest_addrs) {
                send_message(addr, dest_port, msg);
                // Small delay between messages to avoid overwhelming the radio
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        inline std::string get_address() const override {
            std::lock_guard<std::mutex> lock(status_mutex_);
            return current_status_.current_ipv6.empty() ? node_ipv6_ : current_status_.current_ipv6;
        }

        inline uint16_t get_port() const override {
            return 0; // LoRa doesn't use ports
        }

        inline std::string get_interface_name() const override { return interface_name_; }

        inline void set_message_callback(
            std::function<void(const std::string &, const std::string &, uint16_t)> callback) override {
            message_callback_ = callback;
        }

        // LoRa-specific methods
        inline bool set_node_ipv6(const std::string &ipv6_addr) {
            if (!is_valid_ipv6(ipv6_addr)) {
                return false;
            }

            auto ipv6_bytes = string_to_ipv6_bytes(ipv6_addr);
            if (ipv6_bytes.size() != 16) {
                return false;
            }

            if (send_command(CMD_SET_IPV6, ipv6_bytes)) {
                node_ipv6_ = ipv6_addr;
                std::cout << "LoRa node IPv6 address set to: " << ipv6_addr << std::endl;
                return true;
            }

            return false;
        }

        inline LoRaStatus get_status() {
            LoRaStatus status = {};

            if (!running_ || !serial_connected_) {
                return status;
            }

            std::vector<uint8_t> response;
            if (send_command(CMD_GET_STATUS) && wait_for_response(CMD_GET_STATUS, response)) {
                if (response.size() >= 25) {
                    // Parse status response: [16 bytes IPv6][1 byte radio][1 byte power][4 bytes freq][1 byte hop][2
                    // bytes uptime]
                    std::vector<uint8_t> ipv6_bytes(response.begin(), response.begin() + 16);
                    status.current_ipv6 = ipv6_bytes_to_string(ipv6_bytes);
                    status.radio_active = response[16] != 0;
                    status.tx_power = response[17];
                    status.frequency_hz =
                        (response[18] << 24) | (response[19] << 16) | (response[20] << 8) | response[21];
                    status.hop_limit = response[22];
                    status.uptime_seconds = (response[23] << 8) | response[24];

                    std::lock_guard<std::mutex> lock(status_mutex_);
                    current_status_ = status;
                }
            }

            return status;
        }

        inline bool reset_node() { return send_command(CMD_RESET_NODE); }

        inline bool set_tx_power(uint8_t power) {
            std::vector<uint8_t> config_data = {0x01, power}; // Config type 0x01 = TX power
            return send_command(CMD_SET_CONFIG, config_data);
        }

        inline bool set_frequency(uint32_t frequency_hz) {
            std::vector<uint8_t> config_data = {0x02}; // Config type 0x02 = frequency
            config_data.push_back((frequency_hz >> 24) & 0xFF);
            config_data.push_back((frequency_hz >> 16) & 0xFF);
            config_data.push_back((frequency_hz >> 8) & 0xFF);
            config_data.push_back(frequency_hz & 0xFF);
            return send_command(CMD_SET_CONFIG, config_data);
        }

        inline bool set_hop_limit(uint8_t hop_limit) {
            std::vector<uint8_t> config_data = {0x03, hop_limit}; // Config type 0x03 = hop limit
            return send_command(CMD_SET_CONFIG, config_data);
        }

        // Connection management
        inline bool is_connected() const override { return running_ && serial_connected_; }

        inline void set_command_timeout(std::chrono::milliseconds timeout) { command_timeout_ = timeout; }

        // Message retrieval (non-blocking)
        inline bool has_messages() const {
            std::lock_guard<std::mutex> lock(message_queue_mutex_);
            return !incoming_messages_.empty();
        }

        inline std::vector<IncomingMessage> get_pending_messages() {
            std::lock_guard<std::mutex> lock(message_queue_mutex_);
            std::vector<IncomingMessage> messages;

            while (!incoming_messages_.empty()) {
                messages.push_back(incoming_messages_.front());
                incoming_messages_.pop();
            }

            return messages;
        }
    };

} // namespace impulse
