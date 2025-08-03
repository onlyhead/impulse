# Protobuf Integration Example

This example demonstrates how users can integrate their own Protocol Buffer messages with the impulse networking library.

## Overview

The impulse library provides a generic `ProtobufWrapper<T>` that can wrap **any** user-defined protobuf message and make it work seamlessly with both LAN and LoRa networks.

## How it Works

1. **User creates their own .proto file** (like `user_message.proto`)
2. **Library provides generic wrapper** that works with any protobuf message
3. **Automatic network serialization** with size prefixes for reliable transmission
4. **Conditional compilation** - protobuf support is optional

## Usage Pattern

```cpp
#include "impulse/protocol/protobuf/protobuf.hpp"
#include "my_custom_message.pb.h"  // User's generated protobuf

// Create wrapper for any protobuf message
auto wrapper = protobuf::create<MyCustomMessage>();

// Use normal protobuf API
wrapper->proto().set_field1("value");
wrapper->proto().set_field2(42);

// Send via any network interface
lan.send_message(dest, port, wrapper->serialize_for_network());
lora.multicast_message(wrapper->serialize_for_network());
```

## Building with Protobuf Support

### Prerequisites
- CMake 3.15+
- C++17 compiler
- Git (for FetchContent to download protobuf)

### Build Commands

```bash
# Configure with protobuf support
cmake -B build -DENABLE_PROTOBUF=ON

# Build
cmake --build build

# Run example
./build/examples/protobuf_example/protobuf_user_example
```

### Building without Protobuf

```bash
# Configure without protobuf (default)
cmake -B build

# Build - example will compile but show protobuf disabled message
cmake --build build
./build/examples/protobuf_example/protobuf_user_example
```

## Example Messages

The example includes custom protobuf messages for:

- **RobotStatus**: Robot state including position, battery, missions
- **SensorData**: Various sensor readings (temperature, camera, lidar)

## Network Integration

### LAN Network
- **IPv6 multicast** for broadcasting
- **Point-to-point** messaging
- **Automatic size prefixing** for reliable parsing

### LoRa Network  
- **Compact binary format** optimized for small payloads
- **Broadcast and unicast** support
- **Size optimization** critical for LoRa constraints (51-255 bytes)

## Message Size Considerations

### For LoRa Networks
- Keep messages small (< 200 bytes including size prefix)
- Use simple field types when possible
- Consider message splitting for large data

### For LAN Networks
- Size is less constrained
- Can include larger payloads (images, arrays, etc.)
- Network MTU limits still apply

## Advanced Usage

### Custom Timestamp Handling
The wrapper automatically looks for common timestamp field names:
- `timestamp`
- `time` 
- `created_at`
- `updated_at`

### Message Reception
```cpp
// Set up callback to handle incoming messages
lan.set_message_callback([](const std::string& data, const std::string& from, uint16_t port) {
    auto msg = ProtobufWrapper<MyMessage>::from_network_data(data);
    if (msg) {
        // Process received protobuf message
        std::cout << "Received: " << msg->proto().field1() << std::endl;
    }
});
```

## Benefits of This Approach

1. **User Flexibility**: Users define their own message schemas
2. **Library Simplicity**: Library provides generic wrapper, not specific messages
3. **Optional Dependency**: Protobuf is completely optional
4. **Network Agnostic**: Same message format works with LAN and LoRa
5. **Type Safety**: Full compile-time protobuf validation