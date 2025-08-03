# Impulse - Generic Network Communication Library

A header-only C++ library for building distributed systems with automatic agent discovery and continuous messaging capabilities.

## Features

- **Generic Transport Layer**: Template-based transport supporting any message type
- **Continuous Broadcasting**: Automatic periodic message transmission in background threads
- **Agent Discovery**: Built-in peer discovery with join time tracking
- **Network Interface Abstraction**: Support for LAN and LoRa (via melodi firmware) network types
- **Thread-Safe Operations**: Concurrent message sending and receiving
- **Header-Only**: Easy integration, no separate compilation required
- **LoRa Mesh Support**: IPv6-based mesh networking over LoRa with automatic fragmentation and hop-based forwarding

## Quick Start

### 1. Define Your Message Type

```cpp
#include "impulse/protocol/message.hpp"

struct MyDiscoveryMessage : public Message {
    uint64_t timestamp;
    uint64_t join_time;
    char device_id[32];
    int32_t capability;
    
    // Required Message interface implementations
    void serialize(char *buffer) const override { memcpy(buffer, this, sizeof(MyDiscoveryMessage)); }
    void deserialize(const char *buffer) override { memcpy(this, buffer, sizeof(MyDiscoveryMessage)); }
    uint32_t get_size() const override { return sizeof(MyDiscoveryMessage); }
    void set_timestamp(uint64_t ts) override { this->timestamp = ts; }
    std::string to_string() const override {
        return "Device{id=" + std::string(device_id) + ", cap=" + std::to_string(capability) + "}";
    }
};
```

### 2. Create Network Interface

```cpp
#include "impulse/network/lan.hpp"
#include "impulse/network/lora.hpp"

// Use LAN interface
auto lan_interface = std::make_unique<LanInterface>("eth0");

// Use LoRa interface (requires melodi firmware device)
auto lora_interface = std::make_unique<LoRaInterface>(
    "/dev/ttyACM0",           // Serial port
    lan_interface->get_address()  // IPv6 address (same as LAN)
);
```

### 3. Set Up Continuous Transport

```cpp
#include "impulse/protocol/transport.hpp"

class MyAgent {
private:
    std::string name_;
    Transport<MyDiscoveryMessage> transport_;
    std::map<std::string, MyDiscoveryMessage> known_peers_;
    mutable std::mutex peers_mutex_;

public:
    MyAgent(const std::string& name, NetworkInterface* network_interface) 
        : name_(name), 
          // Enable continuous broadcasting every 2 seconds
          transport_(name, network_interface, 100, true, std::chrono::milliseconds(2000)) {
        
        // Set up message handler for incoming messages
        transport_.set_message_handler([this](const MyDiscoveryMessage& msg, const std::string& from_addr) {
            handle_peer_message(msg, from_addr);
        });
        
        // Create initial discovery message
        MyDiscoveryMessage discovery_msg = {};
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        discovery_msg.timestamp = now;
        discovery_msg.join_time = now;  // Set once, never changes
        discovery_msg.capability = 100;
        strncpy(discovery_msg.device_id, transport_.get_address().c_str(), 31);
        
        // Add self to known peers
        {
            std::lock_guard<std::mutex> lock(peers_mutex_);
            known_peers_[transport_.get_address()] = discovery_msg;
        }
        
        // Set message for continuous broadcasting
        transport_.set_broadcast_message(discovery_msg);
        
        // Start transport (begins continuous broadcasting and message monitoring)
        transport_.start();
    }
    
    ~MyAgent() {
        transport_.stop();
    }
    
    void update_capability(int32_t new_capability) {
        MyDiscoveryMessage updated_msg = {};
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        updated_msg.timestamp = now;  // Current time
        
        // Preserve original join_time
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto self_peer = known_peers_.find(transport_.get_address());
        if (self_peer != known_peers_.end()) {
            updated_msg.join_time = self_peer->second.join_time;
        } else {
            updated_msg.join_time = now;
        }
        
        updated_msg.capability = new_capability;
        strncpy(updated_msg.device_id, transport_.get_address().c_str(), 31);
        
        // Update broadcast message (will be sent continuously)
        transport_.set_broadcast_message(updated_msg);
    }
    
    void print_network_status() const {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
            
        std::cout << "=== Network Status ===" << std::endl;
        for (const auto& [device_id, peer] : known_peers_) {
            auto join_time_seconds = (now - peer.join_time) / 1000;
            std::cout << "  - " << peer.to_string() << " joined " << join_time_seconds << "s ago" << std::endl;
        }
    }

private:
    void handle_peer_message(const MyDiscoveryMessage& msg, const std::string& from_addr) {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        std::string device_id(msg.device_id);
        
        if (known_peers_.find(device_id) == known_peers_.end()) {
            std::cout << "New peer discovered: " << msg.to_string() << std::endl;
        }
        
        known_peers_[device_id] = msg;
    }
};
```

### 4. Usage Example

```cpp
int main() {
    // Create network interface
    auto lan_interface = std::make_unique<LanInterface>();
    
    // Create agent with continuous discovery
    MyAgent agent("Device1", lan_interface.get());
    
    // Agent automatically broadcasts discovery messages every 2 seconds
    // and monitors for incoming messages in background threads
    
    while (true) {
        agent.print_network_status();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Update capability - will be broadcast automatically
        agent.update_capability(rand() % 200);
    }
    
    return 0;
}
```

## Architecture

### Transport Layer
- **Template-based**: `Transport<MessageT>` works with any message type
- **Continuous Mode**: Optional automatic periodic broadcasting
- **Thread Management**: Handles message loop and network callbacks internally
- **Configurable Intervals**: Set custom broadcast frequencies

### Network Interfaces

#### Current: LAN Interface
```cpp
auto lan = std::make_unique<LanInterface>();
```

#### LoRa Interface
```cpp
// LoRa interface via serial connection to melodi firmware device
auto lora = std::make_unique<LoRaInterface>(
    "/dev/ttyACM0",         // Serial port connected to LoRa device
    "fd00:dead:beef::1"     // IPv6 address (should match LAN interface)
);

// Start the interface
if (!lora->start()) {
    std::cerr << "Failed to start LoRa interface" << std::endl;
    return 1;
}

// Configure LoRa parameters (optional)
lora->set_tx_power(20);                    // TX power in dBm
lora->set_frequency(868000000);            // Frequency in Hz (868 MHz for Europe)
lora->set_hop_limit(3);                    // Mesh hop limit (1-15)
lora->set_default_repeat_count(2);         // Repeat each fragment (1-255)

// Usage is identical to LAN interface
Transport<MyMessage> transport("device", lora.get(), 100, true, std::chrono::seconds(10));
```

### Message Types
- Inherit from `Message` base class
- Implement serialization, deserialization, and utility methods
- Support timestamp management for continuous broadcasting

## Key Concepts

### Timestamp vs Join Time
- **`timestamp`**: Updated on every broadcast - represents when message was sent
- **`join_time`**: Set once when device starts - represents when device joined network
- Use `join_time` for "device online for X seconds" calculations

### Continuous Broadcasting
- Set `continuous=true` in Transport constructor
- Call `set_broadcast_message()` to define what to broadcast
- Transport automatically sends message at specified intervals
- Update broadcast message anytime with `set_broadcast_message()`

### Thread Safety
- All operations are thread-safe
- Message reception happens in background thread
- Broadcasting happens in same background thread
- Use mutexes when accessing shared data structures

## Building

This is a header-only library. Simply include the headers:

```cpp
#include "impulse/protocol/transport.hpp"
#include "impulse/protocol/message.hpp"
#include "impulse/network/interface.hpp"
```

### Dependencies
- **concord**: Geographic and networking utilities
- **C++20**: Required for template features and chrono utilities
- **melodi firmware**: Required for LoRa interface (see [melodi-net/melodi](https://github.com/melodi-net/melodi))

### CMake Integration
```cmake
find_package(impulse REQUIRED)
target_link_libraries(your_target impulse::impulse)
```

## Examples

See `examples/` directory for complete implementations:
- `examples/aris.cpp` - Basic agent discovery
- `examples/lan_example.cpp` - LAN network demonstration
- `examples/lora_example.cpp` - LoRa mesh network with multi-agent discovery
- `examples/lora_simple_test.cpp` - Simple LoRa send/receive test
- `examples/aris_lora.cpp` - ARIS robot with both LAN and LoRa interfaces

## LoRa Mesh Network Setup

The LoRa interface requires a device running the [melodi firmware](https://github.com/melodi-net/melodi), which implements IPv6-based mesh networking over LoRa radio.

### Hardware Requirements
- LoRa radio module (RFM95, SX1276, etc.) connected via USB serial
- Device appears as `/dev/ttyACM*` or `/dev/ttyUSB*`
- Check with `ls -la /dev/ttyACM*` or `dmesg` after connecting

### LoRa Interface Usage

```cpp
// Basic setup
LoRaInterface lora("/dev/ttyACM0", "fd00:dead:beef::1");
if (!lora.start()) {
    std::cerr << "Failed to start LoRa" << std::endl;
    return 1;
}

// Send message with custom repeat count
lora.send_message_with_repeat("fd00:dead:beef::2", "Hello", 3);

// Broadcast to all nodes
lora.multicast_message("Network announcement");

// Configure parameters
lora.set_tx_power(20);          // 0-23 dBm
lora.set_frequency(868000000);  // Hz (868 MHz EU, 915 MHz US)
lora.set_hop_limit(5);          // 1-15 hops
lora.set_default_repeat_count(2); // 1-255 repeats per fragment
```

### Key Differences from LAN
- **Fragmentation**: Messages are split into 128-byte chunks
- **Mesh Routing**: Packets hop through intermediate nodes
- **Repeat Count**: Each fragment can be repeated for reliability
- **No Ports**: LoRa doesn't use port numbers (always 0)
- **Binary Protocol**: Uses efficient binary serialization

## License

MIT License
