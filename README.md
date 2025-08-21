# High-Frequency Trading (HFT) Socket Server

A high-performance, ultra-low latency socket server designed for high-frequency trading applications, built with C++20 and optimized for sub-20μs latency targets.

## 🚀 Features

- **Ultra-Low Latency**: Target average latency < 20μs
- **High Throughput**: Optimized for high message rates
- **C++20 Modern Features**: Latest language standards and optimizations
- **Singleton Architecture**: Single server instance management
- **Service Pattern**: Modular message processing services
- **Multi-threading**: Efficient concurrent connection handling
- **epoll-based I/O**: Linux-optimized event notification
- **Performance Monitoring**: Real-time latency and throughput metrics

## 🏗️ Architecture

### Core Components

- **HFTServer**: Main server singleton with connection management
- **IMessageService**: Abstract service interface for message processing
- **OrderService**: Handles order-related messages
- **MarketDataService**: Processes market data updates
- **Connection**: Client connection management with socket optimization

### Design Patterns

- **Singleton Pattern**: Ensures single server instance
- **Service Pattern**: Decoupled message processing logic
- **Observer Pattern**: Event-driven message handling

## 📋 Requirements

### System Requirements
- Linux kernel with epoll support
- GCC 10+ or Clang 12+ with C++20 support
- 64-bit architecture recommended

### Dependencies
- Standard C++20 library
- POSIX threads (pthread)
- Linux system calls (socket, epoll, etc.)

## 🛠️ Building

### Option 1: Direct g++ Compilation (Recommended)

```bash
# Make build script executable
chmod +x build.sh

# Build all components
./build.sh
```

This will build:
- `hft_server` - Standard HFT server
- `ultra_hft_server` - Ultra-optimized HFT server with lock-free queues
- `test_client` - Standard test client
- `ultra_test_client` - Ultra-optimized test client

### Option 2: CMake Build

```bash
# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Install (optional)
sudo make install
```

## 🚀 Running

### Starting the HFT Server

```bash
# Standard HFT Server
./build/bin/hft_server --port 8888 --threads 4

# Ultra HFT Server (Lock-free optimized)
./build/bin/ultra_hft_server --port 9999 --threads 8

# Help and options
./build/bin/hft_server --help
./build/bin/ultra_hft_server --help
```

### Server Options

- `--port <port>`: Server port (default: 8888)
- `--threads <n>`: Number of worker threads (default: 4)
- `--help`: Show help message

## 🧪 Testing

### Standard Test Client

The `test_client` provides comprehensive testing capabilities for the standard HFT server.

#### Basic Usage

```bash
# Show help
./build/bin/test_client --help

# Comprehensive test mode
./build/bin/test_client --mode comprehensive

# Specific test modes
./build/bin/test_client --mode performance
./build/bin/test_client --mode market_data
./build/bin/test_client --mode heartbeat
```

#### Test Modes

1. **Comprehensive Mode** (`--mode comprehensive`)
   - Runs all test types sequentially
   - Performance, market data, and heartbeat tests
   - Full system validation

2. **Performance Mode** (`--mode performance`)
   - Latency and throughput testing
   - Message rate analysis
   - Performance benchmarking

3. **Market Data Mode** (`--mode market_data`)
   - Market data message testing
   - Bid/ask spread simulation
   - Volume and price updates

4. **Heartbeat Mode** (`--mode heartbeat`)
   - Connection health monitoring
   - Keep-alive message testing
   - Connection stability validation

#### Advanced Options

```bash
# Custom server configuration
./build/bin/test_client --ip 127.0.0.1 --port 8888 --mode comprehensive

# Performance test with custom parameters
./build/bin/test_client --mode performance --count 10000 --delay 1

# Market data test with specific symbols
./build/bin/test_client --mode market_data --symbols AAPL,MSFT,GOOGL
```

### Ultra Test Client

The `ultra_test_client` is specifically designed for testing the ultra HFT server with advanced performance measurement capabilities.

#### Basic Usage

```bash
# Show help
./build/bin/ultra_test_client --help

# Ultra-low latency test
./build/bin/ultra_test_client --mode latency --count 10000

# High throughput test
./build/bin/ultra_test_client --mode throughput --count 50000

# Stress test
./build/bin/ultra_test_client --mode stress --duration 300 --rate 5000

# Market data streaming
./build/bin/ultra_test_client --mode streaming --duration 120 --rate 1000
```

#### Test Modes

1. **Latency Mode** (`--mode latency`)
   - **Target**: Sub-10μs latency measurement
   - **Features**: Nanosecond precision timing
   - **Usage**: `--count <n>` for message count
   - **Output**: Detailed latency statistics

2. **Throughput Mode** (`--mode throughput`)
   - **Target**: Maximum messages per second
   - **Features**: Burst processing analysis
   - **Usage**: `--count <n>` and `--burst <size>`
   - **Output**: Throughput metrics and burst timing

3. **Stress Mode** (`--mode stress`)
   - **Target**: Sustained high-load testing
   - **Features**: Rate-limited message sending
   - **Usage**: `--duration <seconds>` and `--rate <msgs/sec>`
   - **Output**: Load sustainability analysis

4. **Streaming Mode** (`--mode streaming`)
   - **Target**: Real-time market data simulation
   - **Features**: Continuous data flow
   - **Usage**: `--duration <seconds>` and `--rate <updates/sec>`
   - **Output**: Streaming performance metrics

#### Advanced Options

```bash
# Custom server configuration
./build/bin/ultra_test_client --ip 127.0.0.1 --port 9999 --mode latency

# Latency test with custom parameters
./build/bin/ultra_test_client --mode latency --count 10000 --delay 1

# Throughput test with burst size
./build/bin/ultra_test_client --mode throughput --count 100000 --burst 1000

# Stress test with high load
./build/bin/ultra_test_client --mode stress --duration 600 --rate 10000

# Streaming with high update rate
./build/bin/ultra_test_client --mode streaming --duration 300 --rate 5000
```

## 📊 Performance Monitoring

### Real-time Statistics

Both servers provide real-time performance metrics:

```
=== HFT Server Statistics ===
Total Messages: 15432
Active Connections: 5
Peak Connections: 8
Average Latency: 18.45 μs
✓ Low latency target met (< 20μs)
=================================
```

### Ultra HFT Server Statistics

```
=== Ultra HFT Server Statistics ===
Total Messages: 25467
Active Connections: 3
Peak Connections: 12
Average Latency: 8.92 μs
✓ Ultra-low latency target met (< 10μs)
=================================
```

## 🔧 Configuration

### Socket Options

- **TCP_NODELAY**: Disables Nagle's algorithm for low latency
- **SO_REUSEADDR**: Allows port reuse for rapid restarts
- **SO_KEEPALIVE**: Maintains connection health
- **SO_SNDBUF/SO_RCVBUF**: Optimized buffer sizes

### Threading Configuration

- **Worker Threads**: Configurable thread pool size
- **Connection Handling**: One thread per connection
- **Message Processing**: Asynchronous message handling

## 📈 Performance Characteristics

### Latency Targets

- **Standard HFT Server**: < 20μs average latency
- **Ultra HFT Server**: < 10μs average latency
- **Measurement**: Nanosecond precision timing

### Throughput Capabilities

- **Standard Server**: 10,000+ messages/second
- **Ultra Server**: 50,000+ messages/second
- **Scalability**: Linear scaling with thread count

### Memory Usage

- **Standard Server**: ~60KB executable
- **Ultra Server**: ~48KB executable
- **Test Clients**: ~116KB each

## 🚨 Troubleshooting

### Common Issues

1. **Permission Denied**
   ```bash
   # Fix build directory permissions
   sudo rm -rf build
   ./build.sh
   ```

2. **Connection Refused**
   ```bash
   # Check if server is running
   ps aux | grep hft_server
   
   # Start server on correct port
   ./build/bin/hft_server --port 8888
   ```

3. **Compilation Errors**
   ```bash
   # Ensure C++20 support
   g++ --version
   
   # Clean and rebuild
   rm -rf build && ./build.sh
   ```

### Performance Issues

1. **High Latency**
   - Check CPU frequency scaling
   - Verify network configuration
   - Monitor system load

2. **Low Throughput**
   - Increase thread count
   - Check buffer sizes
   - Monitor memory usage

## 🔍 Debugging

### Verbose Logging

```bash
# Enable debug output
export HFT_DEBUG=1
./build/bin/hft_server --port 8888
```

### Performance Profiling

```bash
# Profile with perf
sudo perf record -g ./build/bin/ultra_hft_server
sudo perf report

# Profile with gprof
g++ -pg -O2 -o hft_server_debug *.cpp
./hft_server_debug
gprof hft_server_debug gmon.out
```

## 📚 API Reference

### Message Types

```cpp
// Base message structure
struct Message {
    uint64_t message_id;
    uint64_t timestamp;
    uint32_t message_type;
    uint32_t payload_size;
    std::array<uint8_t, 1024> payload;
};

// Order message
struct OrderMessage : public Message {
    char symbol[16];
    uint32_t side;      // 0=BUY, 1=SELL
    uint64_t quantity;
    uint64_t price;
    uint32_t order_type;
    uint32_t time_in_force;
};

// Market data message
struct MarketDataMessage : public Message {
    char symbol[16];
    uint64_t bid_price;
    uint64_t bid_size;
    uint64_t ask_price;
    uint64_t ask_size;
    uint64_t last_price;
    uint64_t volume;
};
```

### Service Interface

```cpp
class IMessageService {
public:
    virtual ~IMessageService() = default;
    virtual void process_message(const Message& msg, Connection& conn) = 0;
    virtual std::string get_service_name() const = 0;
};
```

## 🌟 Advanced Features

### Ultra HFT Server Features

- **Lock-free Queues**: Atomic operations for maximum performance
- **Cache-line Alignment**: Optimized memory access patterns
- **Zero-copy Processing**: Minimal memory allocations
- **Pre-allocated Buffers**: Fixed-size memory pools
- **Atomic Statistics**: Thread-safe performance metrics

### Test Client Features

- **Multi-mode Testing**: Comprehensive test scenarios
- **Performance Metrics**: Detailed latency and throughput analysis
- **Real-time Monitoring**: Live performance tracking
- **Customizable Parameters**: Configurable test parameters
- **Error Handling**: Robust error detection and reporting

## 🤝 Contributing

### Development Setup

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

### Code Style

- Follow C++20 best practices
- Use meaningful variable names
- Add comprehensive comments
- Include error handling
- Maintain performance focus

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgments

- Linux kernel team for epoll support
- C++ Standards Committee for C++20 features
- High-frequency trading community for performance insights

## 📞 Support

For questions, issues, or contributions:

- Create an issue on GitHub
- Check the troubleshooting section
- Review the API documentation
- Test with provided test clients

---

**Performance Disclaimer**: Actual latency and throughput may vary based on hardware, system configuration, and network conditions. The ultra HFT server is designed for production environments with proper tuning and monitoring. 