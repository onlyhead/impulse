#pragma once

#ifdef ENABLE_PROTOBUF

#include "impulse/protocol/message.hpp"
#include <google/protobuf/message.h>
#include <memory>
#include <string>
#include <cstring>

namespace impulse {

    /**
     * @brief Generic wrapper for ANY user-defined protobuf message
     * 
     * Users provide their own .proto files and generated C++ classes.
     * This wrapper integrates them with the impulse network protocol.
     * 
     * Example usage:
     *   // User has: my_message.proto -> my_message.pb.h
     *   ProtobufWrapper<MyMessage> wrapper;
     *   wrapper.proto().set_field1("value");
     *   wrapper.proto().set_field2(42);
     *   
     *   // Send via network
     *   lan.send_message(dest, port, wrapper.serialize_for_network());
     */
    template<typename ProtoMessageType>
    class ProtobufWrapper : public Message {
    private:
        std::unique_ptr<ProtoMessageType> proto_msg_;
        
    public:
        ProtobufWrapper() : proto_msg_(std::make_unique<ProtoMessageType>()) {}
        
        explicit ProtobufWrapper(const ProtoMessageType& msg) 
            : proto_msg_(std::make_unique<ProtoMessageType>(msg)) {}
        
        virtual ~ProtobufWrapper() = default;
        
        // Copy constructor
        ProtobufWrapper(const ProtobufWrapper& other) 
            : proto_msg_(std::make_unique<ProtoMessageType>(*other.proto_msg_)) {}
        
        // Assignment operator
        ProtobufWrapper& operator=(const ProtobufWrapper& other) {
            if (this != &other) {
                proto_msg_ = std::make_unique<ProtoMessageType>(*other.proto_msg_);
            }
            return *this;
        }
        
        // Message interface implementation
        void serialize(char* buffer) const override {
            std::string serialized = proto_msg_->SerializeAsString();
            uint32_t size = static_cast<uint32_t>(serialized.size());
            
            // Store size first (4 bytes), then data
            std::memcpy(buffer, &size, sizeof(uint32_t));
            std::memcpy(buffer + sizeof(uint32_t), serialized.data(), size);
        }
        
        void deserialize(const char* buffer) override {
            // Read size first (4 bytes), then data
            uint32_t size;
            std::memcpy(&size, buffer, sizeof(uint32_t));
            
            // Parse the protobuf data
            proto_msg_->ParseFromArray(buffer + sizeof(uint32_t), static_cast<int>(size));
        }
        
        uint32_t get_size() const override {
            // Return total size: 4-byte size prefix + protobuf data
            return sizeof(uint32_t) + static_cast<uint32_t>(proto_msg_->ByteSizeLong());
        }
        
        std::string to_string() const override {
            return proto_msg_->DebugString();
        }
        
        void set_timestamp(uint64_t timestamp) override {
            // Generic timestamp setting - requires protobuf message to have timestamp field
            // Disabled for now to avoid reflection issues
            (void)timestamp; // Suppress unused parameter warning
        }
        
        // Main API: Access the underlying protobuf message
        ProtoMessageType& proto() { return *proto_msg_; }
        const ProtoMessageType& proto() const { return *proto_msg_; }
        
        // Network serialization with size prefix (4 bytes big-endian + payload)
        std::string serialize_for_network() const {
            std::string payload = proto_msg_->SerializeAsString();
            uint32_t size = static_cast<uint32_t>(payload.size());
            
            std::string result;
            result.reserve(4 + payload.size());
            
            // Add size prefix (big-endian)
            result.push_back((size >> 24) & 0xFF);
            result.push_back((size >> 16) & 0xFF);
            result.push_back((size >> 8) & 0xFF);
            result.push_back(size & 0xFF);
            
            result.append(payload);
            return result;
        }
        
        // Deserialize from network data (with size prefix)
        bool deserialize_from_network(const std::string& data) {
            if (data.size() < 4) return false;
            
            // Extract size (big-endian)
            uint32_t size = (static_cast<uint8_t>(data[0]) << 24) |
                           (static_cast<uint8_t>(data[1]) << 16) |
                           (static_cast<uint8_t>(data[2]) << 8) |
                           static_cast<uint8_t>(data[3]);
            
            if (data.size() < 4 + size) return false;
            
            return proto_msg_->ParseFromArray(data.data() + 4, static_cast<int>(size));
        }
        
        // Static factory method for creating from network data
        static std::unique_ptr<ProtobufWrapper<ProtoMessageType>> from_network_data(const std::string& data) {
            auto wrapper = std::make_unique<ProtobufWrapper<ProtoMessageType>>();
            if (wrapper->deserialize_from_network(data)) {
                return wrapper;
            }
            return nullptr;
        }
        
        // Serialize to string (no size prefix)
        std::string serialize_to_string() const {
            return proto_msg_->SerializeAsString();
        }
        
        // Deserialize from string (no size prefix)
        bool deserialize_from_string(const std::string& data) {
            return proto_msg_->ParseFromString(data);
        }
        
    private:
        // Placeholder for future timestamp implementation
        void set_timestamp_if_exists(uint64_t timestamp) {
            // TODO: Implement safe timestamp setting without reflection
            (void)timestamp;
        }
    };

    namespace protobuf {
        
        /**
         * @brief Check if protobuf support is available
         */
        inline bool is_available() { return true; }
        
        /**
         * @brief Get protobuf version information
         */
        std::string get_version() {
            return "Protocol Buffers " + std::to_string(GOOGLE_PROTOBUF_VERSION);
        }
        
        /**
         * @brief Helper function to create a protobuf wrapper from any protobuf message
         */
        template<typename ProtoMessageType>
        std::unique_ptr<ProtobufWrapper<ProtoMessageType>> wrap(const ProtoMessageType& msg) {
            return std::make_unique<ProtobufWrapper<ProtoMessageType>>(msg);
        }
        
        /**
         * @brief Helper function to create an empty protobuf wrapper
         */
        template<typename ProtoMessageType>
        std::unique_ptr<ProtobufWrapper<ProtoMessageType>> create() {
            return std::make_unique<ProtobufWrapper<ProtoMessageType>>();
        }
    }

} // namespace impulse

#else

// Protobuf not available - provide stub implementations
namespace impulse {
    namespace protobuf {
        inline bool is_available() { return false; }
        inline std::string get_version() { return "Protobuf support not compiled"; }
    }
}

#endif // ENABLE_PROTOBUF