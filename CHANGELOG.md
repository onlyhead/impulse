# Changelog

## [0.2.0] - 2025-08-04

### <!-- 0 -->⛰️  Features

- Refactor IPv6 network configuration and setup
- Integrate Protocol Buffers for structured data serialization

### <!-- 1 -->🐛 Bug Fixes

- Refactor: Fix agent instantiation issue

### <!-- 2 -->🚜 Refactor

- Unify transport management and enable IPv6 support
- Refactor: Improve clarity and configurability of LAN interfaces

### Build

- Remove Protobuf from all test files

## [0.1.0] - 2025-08-03

### <!-- 0 -->⛰️  Features

- Integrate LoRa mesh networking and position messaging
- Refactor LoRa stack for improved configuration and reliability
- Add LoRa physical layer communication support
- Implement DHCPv6 client for IPv6 address acquisition
- Refactor agent position tracking with inlined transport
- Broadcast continuous discovery messages for agents
- Refactor network interfaces to use a common base class
- Refactor agent discovery and introduce position tracking
- Refactor robot communications to use new agent architecture
- Implement ARIS robot discovery and communication
- Support IPv6 multicast messaging
- Implement LAN communication and ARIS P2P discovery examples

### <!-- 1 -->🐛 Bug Fixes

- Improve `SerializationType` and `TransportType` string conversions

### <!-- 2 -->🚜 Refactor

- Refactor IPv6 address acquisition with enum
- Report IPv6 setup failures and reformat callback
- Wrap network and protocol classes within impulse namespace
- Refactor transport layer for continuous message broadcasting
- Standardize network agent identification
- Refactor agent and transport for unified message handling
- Refactor agent discovery and communication mechanisms
- Refactor agent and introduce new message types
- Refactor agent and message handling, simplify initialization
- Refactor and clean up Agent and Transport classes
- Update LAN interface name and port configuration
- Refactor ARIS example and header files

### <!-- 3 -->📚 Documentation

- Document Matter over LoRaWAN research and implementation
- Add comprehensive project documentation to README

### Build

- Remove Cap'n Proto dependency fetching


