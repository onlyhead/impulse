# Matter Protocol Research and Implementation Guide

## Overview

Matter is an IPv6-based smart home protocol that provides interoperability between devices from different manufacturers. This document analyzes Matter's architecture and suggests how similar concepts can be applied to a LoRa-based implementation.

## Key Similarities with Our Implementation

1. **IPv6-based**: Both Matter and our impulse protocol use IPv6 as the foundation
2. **Multiple Physical Layers**: Matter supports WiFi/Ethernet/Thread, we use WiFi/Ethernet/LoRa
3. **Local Operation**: Both protocols emphasize local network operation without cloud dependency
4. **Discovery Mechanisms**: Both use mDNS for service discovery

## Matter Architecture Components

### 1. Network Stack

Matter's network stack consists of:

```
Application Layer    → Matter Protocol
Transport Layer      → UDP/TCP
Network Layer        → IPv6
Link Layer          → WiFi/Ethernet/Thread
```

**Key Insights for LoRa Implementation:**
- Matter uses UDP port 5540 for device communication
- Multicast is heavily used for discovery and group communication
- IPv6 link-local addresses are critical for initial communication

### 2. Discovery Mechanisms

Matter uses three mDNS service types:

1. **Commissionable Discovery** (`_matterc._udp`)
   - For uncommissioned devices advertising availability
   - DNS-SD instance names use MAC addresses (e.g., `A5F15790B0D15F32.local`)

2. **Operational Discovery** (`_matter._tcp`)
   - For commissioned devices within a fabric
   - Despite the `_tcp` naming, actually uses UDP transport

3. **Commissioner Discovery** (`_matterd._udp`)
   - For devices that can commission others

**Implementation Suggestions:**
- Define similar service types for LoRa devices:
  - `_impulse-new._udp` for uncommissioned devices
  - `_impulse._udp` for operational devices
  - `_impulse-comm._udp` for commissioners

### 3. Commissioning Process

Matter's commissioning flow:

1. **BLE Advertisement** (first 30 seconds: 20-60ms intervals, then 150-1500ms)
2. **PASE (Password Authenticated Session Establishment)**
   - Uses passcode from QR code
   - Establishes encrypted channel
3. **Network Configuration**
   - Device receives IPv6 address
   - Joins the fabric
4. **Certificate Assignment**
   - Node Operational Certificate (NOC) issued
   - Device becomes part of the fabric

**LoRa Adaptation:**
- Use LoRa for initial discovery instead of BLE
- Implement similar PASE mechanism over LoRa
- Consider using lower data rates for discovery to maximize range

### 4. Security Model

Matter uses a multi-layered security approach:

#### Certificate Hierarchy
- **Root CA Certificate** → Defines a fabric
- **Intermediate Certificates** → Optional delegation
- **Node Operational Certificate (NOC)** → Device identity within fabric
- **Device Attestation Certificate (DAC)** → Manufacturer-provided identity

#### Session Types
1. **PASE Sessions**: Initial commissioning using shared passcode
2. **CASE Sessions**: Operational communication using certificates

**LoRa Security Recommendations:**
- Implement similar two-phase security (commissioning vs operational)
- Use lightweight crypto suitable for LoRa bandwidth constraints
- Consider pre-shared keys for initial authentication

### 5. Data Model

Matter's hierarchical data model:

```
Node (Device)
├── Endpoint 0 (Root/Utility)
│   ├── Basic Information Cluster
│   ├── Network Commissioning Cluster
│   └── OTA Update Cluster
├── Endpoint 1 (Application)
│   ├── On/Off Cluster
│   ├── Level Control Cluster
│   └── Color Control Cluster
└── Endpoint N (Additional Functions)
```

#### Cluster Components
- **Attributes**: Current state (e.g., on/off status)
- **Commands**: Actions (e.g., turn on/off)
- **Events**: Historical logs with timestamps

**LoRa Data Model Suggestions:**
- Keep clusters minimal due to bandwidth constraints
- Prioritize essential attributes and commands
- Implement event compression or selective logging

### 6. Interaction Model

Matter supports five transaction types:

1. **Read**: Get attributes/events
2. **Write**: Modify attributes
3. **Invoke**: Execute commands
4. **Subscribe**: Monitor changes
5. **Timed**: Time-sensitive operations

**LoRa Optimizations:**
- Batch multiple operations in single messages
- Implement efficient subscription mechanisms
- Use delta encoding for attribute updates

### 7. Multicast and Group Communication

Matter uses IPv6 multicast with specific considerations:

- **Multicast Address Formation**: Includes fabric ID in address
- **Scope**: Limited to fabric members
- **Port**: 5540 for Matter multicasts

**Thread Border Router (TBR) Functions:**
- Filters multicast between Thread and WiFi/Ethernet
- Acts as mDNS proxy for Thread devices
- Manages IPv6 prefix delegation

**LoRa Border Router Considerations:**
- Implement similar filtering to prevent LoRa network flooding
- Act as proxy between LoRa and IP networks
- Manage efficient routing between different media

## Detailed IPv6 Implementation

### IPv6 Address Types in Matter/Thread

Matter devices implement multiple IPv6 address types, each serving specific purposes:

#### 1. Link-Local Addresses (LLA)
- **Prefix**: `fe80::/64`
- **Purpose**: Communication within single network segment
- **Formation**: Automatically configured on interface startup
- **Interface ID**: Generated using modified EUI-64 or random method

#### 2. Unique Local Addresses (ULA)
- **Prefix**: `fd00::/8` (locally assigned)
- **Purpose**: Local communication within site/fabric
- **Mesh-Local EID**: Thread-specific ULA for mesh communication
- **Example**: `fd5d:443b:99ef::/64` (OMR prefix)

#### 3. Global Unicast Addresses (GUA)
- **Prefix**: Assigned by ISP or network admin
- **Purpose**: Internet-routable communication
- **Formation**: Via SLAAC or DHCPv6

### IPv6 Address Formation Process

#### SLAAC (Stateless Address Autoconfiguration)

1. **Router Solicitation (RS)**
   - Device sends ICMPv6 RS message to `ff02::2` (all-routers multicast)
   - Requests network configuration information

2. **Router Advertisement (RA)**
   - Router responds with prefix information
   - Contains prefix, prefix length, and flags
   - Example: `2001:db8:1234::/64`

3. **Address Generation**
   - Device combines prefix with Interface ID
   - Interface ID options:
     - Modified EUI-64 (deprecated for privacy)
     - Random 64-bit value (recommended)

4. **Duplicate Address Detection (DAD)**
   - Send Neighbor Solicitation to solicited-node multicast
   - Address: `ff02::1:ff00:0/104` + last 24 bits of IPv6
   - If no response, address is unique

#### Modified EUI-64 Process (Legacy)

```
MAC Address: 00:1B:44:11:3A:B7

1. Split MAC: 00:1B:44 | 11:3A:B7
2. Insert FFFE: 00:1B:44:FF:FE:11:3A:B7
3. Flip 7th bit: 02:1B:44:FF:FE:11:3A:B7
4. Result: 021B:44FF:FE11:3AB7
```

### Matter IPv6 Multicast Addressing

Matter follows RFC 3306 for multicast address construction:

#### Address Format
```
FF35:0040:FD<Fabric ID>00:<Group ID>
```

#### Breakdown:
- `FF` - Multicast prefix
- `3` - Flags (unicast-prefix-based)
- `5` - Scope (site-local)
- `0040` - Prefix length (64 bits)
- `FD` - ULA prefix indicator
- `<Fabric ID>` - 64-bit fabric identifier
- `<Group ID>` - 16-bit group identifier

#### Special Groups:
- All-nodes in fabric: Group ID `0xFFFF`
- All-power-capable nodes: Group ID `0xFFFE`

### Thread Border Router IPv6 Management

#### Prefix Delegation Process

1. **DHCPv6-PD Request**
   - TBR requests prefix from infrastructure router
   - Receives delegated prefix (e.g., `2001:db8::/48`)

2. **Prefix Subdivision**
   - OMR (Off-Mesh-Routable) prefix for Thread network
   - On-link prefix for infrastructure network

3. **Prefix Advertisement**
   - Thread network: Via Thread protocol
   - Infrastructure: Via Router Advertisements

#### Fallback Mechanism
- If DHCPv6-PD fails, generate random ULA prefix
- Example: `fd12:3456:789a::/64`

### LoRa IPv6 Implementation Recommendations

#### Address Allocation Strategy

1. **Link-Local Addresses**
   ```
   - Prefix: fe80::/64
   - Interface ID: Based on LoRa device EUI or random
   - Used for: Initial discovery and local communication
   ```

2. **Unique Local Addresses**
   ```
   - Prefix: fd00::/8 + 40-bit random
   - Example: fd4c:6f72:6100::/48
   - Subnet for LoRa: fd4c:6f72:6100:1::/64
   ```

3. **Address Generation for LoRa**
   - Use LoRa DevEUI as base for Interface ID
   - Or generate random IDs for privacy
   - Implement lightweight DAD over LoRa

#### LoRa-Specific Optimizations

1. **Compressed Headers**
   - Use 6LoWPAN-style compression
   - Omit common prefix bytes
   - Use short addresses when possible

2. **Efficient Multicast**
   ```
   Multicast Format: FF35:0040:FD<Network ID>00:<Group>
   - Network ID: 64-bit LoRa network identifier
   - Group: 16-bit group ID
   ```

3. **Border Router Functions**
   ```
   LoRa Side:
   - Manage compressed addresses
   - Filter unnecessary multicast
   - Cache frequently used addresses
   
   IP Side:
   - Full IPv6 addresses
   - mDNS proxy services
   - Prefix delegation client
   ```

### Discovery Process Implementation

#### mDNS over LoRa Optimization

1. **Service Advertisement**
   ```
   _impulse-new._udp.local. → Uncommissioned devices
   _impulse._udp.local. → Operational devices
   
   Instance: <DevEUI>.local.
   ```

2. **Compressed mDNS**
   - Use binary encoding instead of DNS format
   - Cache common service types
   - Implement query aggregation

3. **Discovery Timing**
   ```
   Initial: Every 1-5 seconds (high power)
   Normal: Every 30-60 seconds (balanced)
   Idle: Every 5-10 minutes (low power)
   ```

### Security Considerations

1. **Address Privacy**
   - Rotate Interface IDs periodically
   - Use temporary addresses for outgoing connections
   - Implement privacy extensions (RFC 4941)

2. **Secure Neighbor Discovery**
   - Implement SEND (RFC 3971) concepts adapted for LoRa
   - Authenticate Router Advertisements
   - Protect against address spoofing

### Implementation Checklist

- [ ] Implement IPv6 address generation for LoRa devices
- [ ] Create SLAAC mechanism over LoRa
- [ ] Design compressed DAD for bandwidth efficiency
- [ ] Implement multicast address formation with network IDs
- [ ] Create border router with prefix delegation
- [ ] Optimize mDNS for LoRa constraints
- [ ] Test interoperability with standard IPv6 networks

## Implementation Roadmap

### Phase 1: Core Protocol
1. IPv6 addressing scheme for LoRa devices
2. Basic discovery using mDNS
3. Simple commissioning flow

### Phase 2: Security
1. PASE-like authentication over LoRa
2. Certificate management system
3. Secure session establishment

### Phase 3: Data Model
1. Define cluster hierarchy
2. Implement attribute/command/event system
3. Create standard device types

### Phase 4: Advanced Features
1. Group communication
2. OTA updates over LoRa
3. Border router implementation

## Key Differences to Consider

1. **Bandwidth**: LoRa has much lower bandwidth than WiFi/Thread
   - Minimize message sizes
   - Use compression where possible
   - Prioritize essential data

2. **Latency**: LoRa has higher latency
   - Design for asynchronous operations
   - Implement appropriate timeouts
   - Consider duty cycle limitations

3. **Range**: LoRa has much longer range
   - Design discovery for wide-area networks
   - Consider multi-hop scenarios
   - Implement efficient routing

## Practical Implementation Tips

### Discovery Optimization
- Use longer advertisement intervals to conserve power
- Implement adaptive discovery rates based on network activity
- Consider using different LoRa spreading factors for discovery vs data

### Message Efficiency
- Define compact binary protocols
- Use bit-packing for boolean attributes
- Implement message aggregation for multiple commands

### Network Management
- Implement network partitioning for large deployments
- Use hierarchical addressing to simplify routing
- Consider mesh networking for reliability

## References and Standards

- Matter Specification 1.4 (November 2024)
- RFC 3306: Unicast-Prefix-based IPv6 Multicast
- RFC 4291: IPv6 Addressing Architecture
- Thread Specification (for border router concepts)

## Next Steps

1. Define specific mDNS service types for impulse protocol
2. Design commissioning flow adapted for LoRa constraints
3. Create minimal cluster definitions for common device types
4. Implement proof-of-concept for discovery and commissioning
5. Test interoperability with existing IPv6 infrastructure

## Conclusion

While Matter and our LoRa-based implementation have different physical layer characteristics, many of Matter's architectural decisions can be adapted. The key is to maintain the spirit of interoperability and local control while optimizing for LoRa's unique constraints of low bandwidth, high latency, and long range.