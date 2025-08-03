# Protobuf User Example
# Shows how users integrate their own .proto files with impulse
cmake_minimum_required(VERSION 3.15)

# Check if protobuf support is enabled
option(ENABLE_PROTOBUF "Enable Protocol Buffers support" OFF)

if(ENABLE_PROTOBUF)
    # Protobuf should already be configured by main CMakeLists.txt
    if(NOT TARGET protobuf::libprotobuf)
        find_package(Protobuf REQUIRED)
    endif()
    
    # Find Abseil (required by modern protobuf)
    if(NOT TARGET absl::base)
        find_package(absl REQUIRED)
    endif()
    
    # Include protobuf directories
    include_directories(${Protobuf_INCLUDE_DIRS})
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    
    # Generate C++ files from user's .proto file using CMake function
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS user_message.proto)
    
    # Create the example executable
    add_executable(protobuf_user_example 
        protobuf_user_example.cpp
        ${PROTO_SRCS}
    )
    
    # Link libraries - THIS IS THE KEY FIX
    target_link_libraries(protobuf_user_example 
        impulse::impulse
        concord::concord
        protobuf::libprotobuf
        absl::log_internal_check_op
        absl::log_internal_message
        absl::base
        absl::strings
        absl::str_format
        absl::cord
    )
    
    # Include directories
    target_include_directories(protobuf_user_example PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}  # For generated .pb.h files
        ${CMAKE_SOURCE_DIR}/include  # For impulse headers
    )
    
    # Add compile definition
    target_compile_definitions(protobuf_user_example PRIVATE ENABLE_PROTOBUF)
    
    # Set C++17 standard
    target_compile_features(protobuf_user_example PRIVATE cxx_std_17)
    
    # Create the aris_lora_protobuf executable
    add_executable(aris_lora_protobuf 
        aris_lora_protobuf.cpp
        ${PROTO_SRCS}
    )
    
    # Link libraries for aris_lora_protobuf
    target_link_libraries(aris_lora_protobuf 
        impulse::impulse
        concord::concord
        protobuf::libprotobuf
        absl::log_internal_check_op
        absl::log_internal_message
        absl::base
        absl::strings
        absl::str_format
        absl::cord
    )
    
    # Include directories for aris_lora_protobuf
    target_include_directories(aris_lora_protobuf PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}  # For generated .pb.h files
        ${CMAKE_SOURCE_DIR}/include  # For impulse headers
    )
    
    # Add compile definition for aris_lora_protobuf
    target_compile_definitions(aris_lora_protobuf PRIVATE ENABLE_PROTOBUF)
    
    # Set C++17 standard for aris_lora_protobuf
    target_compile_features(aris_lora_protobuf PRIVATE cxx_std_17)
    
    message(STATUS "Protobuf user example and aris_lora_protobuf configured")
    
else()
    # Create a simple executable that shows protobuf is disabled
    add_executable(protobuf_user_example protobuf_user_example.cpp)
    target_include_directories(protobuf_user_example PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(protobuf_user_example impulse)
    
    message(STATUS "Protobuf user example configured (protobuf disabled)")
endif()

# Custom target to generate protobuf files manually (optional)
if(ENABLE_PROTOBUF)
    add_custom_target(generate_user_protobuf
        DEPENDS ${PROTO_SRCS} ${PROTO_HDRS}
        COMMENT "Protobuf files generated"
    )
endif()
