# EtherForge

EtherForge is an open-source EtherCAT master backend that provides a modern, unified interface for EtherCAT network control, monitoring, and development. Built as the foundation of the EtherForge ecosystem, it bridges the gap between low-level EtherCAT master libraries and high-level automation applications.

## Features

- **Multi-threaded Architecture**: Real-time thread, network thread, and management thread
- **UDP Protocol Interface**: Lightweight protocol on port 2346 for real-time performance
- **EtherCAT Master Integration**: SOEM library support with fallback stub implementation
- **Multi-client Support**: Up to 32 concurrent client connections
- **Real-time Capable**: Configurable RT priority and CPU affinity
- **Cross-platform**: Linux support with real-time kernel compatibility

## Quick Start

### Prerequisites

- Linux system (Ubuntu 20.04+, RHEL 8+, or compatible)
- CMake 3.16+
- Ninja build system
- C compiler (GCC/Clang)
- libyaml development library

### Installation

1. **Install build dependencies:**
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake ninja-build libyaml-dev
   ```

2. **Clone and build:**
   ```bash
   git clone <repository-url>
   cd etherforged
   ./build.sh
   ```

3. **Install system-wide (optional):**
   ```bash
   ./build.sh --install
   ```

### Basic Usage

1. **Check installation:**
   ```bash
   ./build/etherforged --version
   ```

2. **Run with default settings:**
   ```bash
   sudo ./build/etherforged --verbose
   ```

3. **Run with specific interface:**
   ```bash
   sudo ./build/etherforged --interface eth1 --verbose
   ```

## Configuration

### Configuration File

EtherForge uses YAML configuration files. Default location: `/etc/etherforged/etherforged.yaml`

Example configuration:
```yaml
network:
  interface: "eth1"
  cycle_time_us: 1000
  timeout_ms: 1000

performance:
  rt_priority: 99
  cpu_affinity: [2, 3]
  buffer_size: 8192

logging:
  level: "info"
  file: "/var/log/etherforged.log"
  max_size: "100MB"

security:
  bind_address: "0.0.0.0"
  port: 2346
  max_clients: 32
```

### Command Line Options

```
Options:
  -c, --config FILE    Configuration file path
  -i, --interface IF   Network interface name (overrides config)
  -p, --port PORT      UDP port number (overrides config)
  -v, --verbose        Enable verbose logging
  -h, --help           Show help message
  --version            Show version information
```

## Protocol Overview

EtherForge uses a UDP-based protocol for client communication:

### Command Structure
```c
typedef struct {
    uint32_t magic;        // 0xEF000001
    uint8_t command_type;  // Command category
    uint8_t command_id;    // Specific operation
    uint16_t payload_len;  // Data length
    uint8_t payload[8];    // Command data
} udp_command_t;
```

### Response Structure
```c
typedef struct {
    uint32_t magic;        // 0xEF800001
    uint8_t status;        // SUCCESS=0, ERROR=1
    uint8_t error_code;    // Specific error code
    uint16_t payload_len;  // Response data length
    uint8_t payload[8];    // Response data
} udp_response_t;
```

### Command Categories

#### Network Commands (0x01)
- `NET_START` (0x01): Initialize EtherCAT network
- `NET_STOP` (0x02): Shutdown network gracefully
- `NET_SCAN` (0x03): Discover and enumerate slaves
- `NET_STATUS` (0x04): Get current network status

#### PDO Commands (0x02)
- `PDO_READ` (0x01): Read process data from slave
- `PDO_WRITE` (0x02): Write process data to slave
- `PDO_MONITOR` (0x03): Start real-time monitoring
- `PDO_STOP_MON` (0x04): Stop monitoring

#### Diagnostic Commands (0x03)
- `DIAG_NETWORK` (0x01): Get network health metrics
- `DIAG_TIMING` (0x02): Get timing analysis data
- `DIAG_ERRORS` (0x03): Get error history
- `DIAG_SLAVE` (0x04): Get individual slave diagnostics

## Client Libraries

### Python Example
```python
import socket
import struct

# Connect to ethercatforge instance
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_address = ('localhost', 2346)

# Create network start command
magic = 0xEF000001
command_type = 0x01  # Network
command_id = 0x01    # Start
payload_len = 0
payload = b'\x00' * 8

cmd = struct.pack('!IBBH8s', magic, command_type, command_id, payload_len, payload)

# Send command and receive response
sock.sendto(cmd, server_address)
response, _ = sock.recvfrom(1024)

# Parse response
resp_magic, status, error_code, resp_len = struct.unpack('!IBBH', response[:8])
print(f"Response: status={status}, error={error_code}")
```

### C Client Example
```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Create socket and connect to etherforge
int sock = socket(AF_INET, SOCK_DGRAM, 0);
struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(2346),
    .sin_addr.s_addr = inet_addr("127.0.0.1")
};

// Send network start command
udp_command_t cmd = {
    .magic = htonl(0xEF000001),
    .command_type = 0x01,
    .command_id = 0x01,
    .payload_len = 0
};

sendto(sock, &cmd, sizeof(cmd), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

// Receive response
udp_response_t resp;
recv(sock, &resp, sizeof(resp), 0);
```

## Architecture

### Thread Model

1. **Network Thread**: Handles UDP communication with clients
2. **Real-time Thread**: Processes EtherCAT cycles at configured intervals
3. **Management Thread**: Handles diagnostics, logging, and housekeeping

### EtherCAT Integration

- **With SOEM**: Full EtherCAT master functionality
- **Stub Mode**: Testing without EtherCAT hardware (automatically used if SOEM not available)

## Development

### Building from Source

```bash
# Debug build
./build.sh --debug

# Clean build
./build.sh --clean

# Install after building
./build.sh --install
```

### Installing SOEM (Optional)

For real EtherCAT functionality, install SOEM:

```bash
git clone https://github.com/OpenEtherCATsociety/SOEM.git
cd SOEM
mkdir build && cd build
cmake ..
make
sudo make install
```

Then rebuild EtherForge to enable SOEM support.

### Project Structure

```
etherforged/
├── src/           # Source code
├── include/       # Header files
├── config/        # Configuration files
├── build/         # Build output
├── CMakeLists.txt # CMake configuration
├── build.sh       # Build script
└── README.md      # This file
```

## System Requirements

### Minimum Requirements
- Linux (Ubuntu 20.04+, RHEL 8+)
- x86_64 dual-core 1GHz CPU
- 512MB RAM
- Gigabit Ethernet adapter

### Recommended for Production
- Real-time Linux kernel (RT_PREEMPT)
- x86_64 quad-core 2GHz+ CPU
- 2GB RAM
- Dedicated EtherCAT network interface
- SSD storage for logging

## Troubleshooting

### Common Issues

1. **Permission denied for real-time priority**
   ```bash
   # Run as root or configure limits
   sudo ./build/etherforged
   ```

2. **Network interface not found**
   ```bash
   # List available interfaces
   ip link show
   # Use correct interface name
   ./build/etherforged --interface enp0s3
   ```

3. **Port already in use**
   ```bash
   # Check what's using port 2346
   sudo netstat -tulpn | grep 2346
   # Use different port
   ./build/etherforged --port 2347
   ```

4. **YAML configuration errors**
   - Check YAML syntax with online validators
   - Ensure proper indentation (spaces, not tabs)
   - Verify file permissions

### Debug Mode

Run with verbose logging to diagnose issues:
```bash
./build/etherforged --verbose --config /path/to/config.yaml
```

### Log Files

Default log location: `/var/log/etherforged.log`

Check logs for detailed error information:
```bash
tail -f /var/log/etherforged.log
```

## Performance Tuning

### Real-time Performance

1. **Use RT kernel**: Install `linux-rt` package
2. **Set CPU isolation**: Add `isolcpus=2,3` to kernel boot parameters
3. **Configure RT priority**: Set `rt_priority: 99` in config
4. **Set CPU affinity**: Use dedicated cores for RT thread

### Network Optimization

1. **Dedicated interface**: Use separate interface for EtherCAT
2. **Interrupt affinity**: Bind network interrupts to specific CPUs
3. **Buffer tuning**: Adjust network buffer sizes if needed

## License

- MIT License

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## Support

- **Documentation**: [docs.etherforge.org](https://docs.etherforge.org)
- **Issues**: [GitHub Issues](https://github.com/etherforge/etherforged/issues)
- **Community**: [forum.etherforge.org](https://forum.etherforge.org)
- **Email**: [support@etherforge.org](mailto:support@etherforge.org)

## Roadmap

- [ ] Web-based management interface
- [ ] Additional EtherCAT master support (IgH, TwinCAT)
- [ ] Advanced diagnostics with ML anomaly detection
- [ ] Cloud connectivity and IoT integration
- [ ] Safety function compliance (SIL2/SIL3)

---

**EtherForge** - Empowering Industrial Automation Development
