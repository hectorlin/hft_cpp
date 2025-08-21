#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <csignal>

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running.store(false);
}

// Colors for output
namespace colors {
    const std::string RED = "\033[0;31m";
    const std::string GREEN = "\033[0;32m";
    const std::string YELLOW = "\033[1;33m";
    const std::string BLUE = "\033[0;34m";
    const std::string MAGENTA = "\033[0;35m";
    const std::string CYAN = "\033[0;36m";
    const std::string WHITE = "\033[1;37m";
    const std::string NC = "\033[0m"; // No Color
}

// Logging functions with colors and timestamps
void log_info(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::cout << colors::BLUE << "[INFO]" << colors::NC << " [" 
              << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "." 
              << std::setfill('0') << std::setw(3) << ms.count() << "] " << message << std::endl;
}

void log_success(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::cout << colors::GREEN << "[SUCCESS]" << colors::NC << " [" 
              << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "." 
              << std::setfill('0') << std::setw(3) << ms.count() << "] " << message << std::endl;
}

void log_error(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::cout << colors::RED << "[ERROR]" << colors::NC << " [" 
              << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "." 
              << std::setfill('0') << std::setw(3) << ms.count() << "] " << message << std::endl;
}

void log_warning(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::cout << colors::YELLOW << "[WARNING]" << colors::NC << " [" 
              << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "." 
              << std::setfill('0') << std::setw(3) << ms.count() << "] " << message << std::endl;
}

void log_performance(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::cout << colors::MAGENTA << "[PERFORMANCE]" << colors::NC << " [" 
              << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "." 
              << std::setfill('0') << std::setw(3) << ms.count() << "] " << message << std::endl;
}

// Ultra HFT message structures (matching ultra_hft_server.h)
struct alignas(64) UltraMessage {
    uint64_t message_id;
    uint64_t timestamp;
    uint32_t message_type;
    uint32_t payload_size;
    std::array<uint8_t, 1024> payload;
    
    UltraMessage() : message_id(0), timestamp(0), message_type(0), payload_size(0) {}
    
    void update_timestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }
    
    static uint64_t get_current_timestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }
};

struct alignas(64) UltraOrderMessage : public UltraMessage {
    char symbol[16];
    uint32_t side;      // 0=BUY, 1=SELL
    uint64_t quantity;
    uint64_t price;
    uint32_t order_type;
    uint32_t time_in_force;
    
    UltraOrderMessage() : UltraMessage(), side(0), quantity(0), price(0), order_type(0), time_in_force(0) {
        message_type = 1; // ORDER_NEW
        std::fill(symbol, symbol + 16, 0);
    }
};

struct alignas(64) UltraMarketDataMessage : public UltraMessage {
    char symbol[16];
    uint64_t bid_price;
    uint64_t bid_size;
    uint64_t ask_price;
    uint64_t ask_size;
    uint64_t last_price;
    uint64_t volume;
    
    UltraMarketDataMessage() : UltraMessage(), bid_price(0), bid_size(0), ask_price(0), ask_size(0), last_price(0), volume(0) {
        message_type = 2; // MARKET_DATA
        std::fill(symbol, symbol + 16, 0);
    }
};

// Ultra Test Client class
class UltraTestClient {
private:
    int sock_fd_;
    std::string server_ip_;
    uint16_t server_port_;
    std::atomic<uint64_t> message_id_counter_{1000000};
    
    // Performance metrics
    struct PerformanceMetrics {
        std::atomic<uint64_t> total_messages{0};
        std::atomic<uint64_t> successful_messages{0};
        std::atomic<uint64_t> failed_messages{0};
        std::atomic<uint64_t> total_latency{0};
        std::atomic<uint64_t> min_latency{UINT64_MAX};
        std::atomic<uint64_t> max_latency{0};
        
        void update_latency(uint64_t latency) {
            total_latency.fetch_add(latency);
            total_messages.fetch_add(1);
            
            uint64_t current_min = min_latency.load();
            while (latency < current_min && !min_latency.compare_exchange_weak(current_min, latency)) {}
            
            uint64_t current_max = max_latency.load();
            while (latency > current_max && !max_latency.compare_exchange_weak(current_max, latency)) {}
        }
        
        double get_average_latency() const {
            uint64_t count = total_messages.load();
            if (count == 0) return 0.0;
            return static_cast<double>(total_latency.load()) / count;
        }
        
        void reset() {
            total_messages.store(0);
            successful_messages.store(0);
            failed_messages.store(0);
            total_latency.store(0);
            min_latency.store(UINT64_MAX);
            max_latency.store(0);
        }
    } metrics_;
    
public:
    UltraTestClient(const std::string& ip = "127.0.0.1", uint16_t port = 8888)
        : server_ip_(ip), server_port_(port), sock_fd_(-1) {}
    
    ~UltraTestClient() {
        disconnect();
    }
    
    bool connect() {
        sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd_ < 0) {
            log_error("Failed to create socket: " + std::string(strerror(errno)));
            return false;
        }
        
        // Set socket options for ultra-low latency
        int opt = 1;
        setsockopt(sock_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        server_addr.sin_addr.s_addr = inet_addr(server_ip_.c_str());
        
        if (::connect(sock_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            log_error("Failed to connect: " + std::string(strerror(errno)));
            close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }
        
        log_success("Connected to Ultra HFT Server at " + server_ip_ + ":" + std::to_string(server_port_));
        return true;
    }
    
    void disconnect() {
        if (sock_fd_ >= 0) {
            close(sock_fd_);
            sock_fd_ = -1;
            log_info("Disconnected from Ultra HFT Server");
        }
    }
    
    bool send_ultra_order(const std::string& symbol, uint32_t side, uint64_t quantity, uint64_t price) {
        if (sock_fd_ < 0) return false;
        
        UltraOrderMessage msg;
        msg.message_id = message_id_counter_.fetch_add(1);
        msg.update_timestamp();
        strncpy(msg.symbol, symbol.c_str(), 15);
        msg.symbol[15] = '\0';
        msg.side = side;
        msg.quantity = quantity;
        msg.price = price;
        msg.order_type = 1; // MARKET
        msg.time_in_force = 1; // DAY
        
        ssize_t bytes_sent = send(sock_fd_, &msg, sizeof(msg), MSG_DONTWAIT);
        if (bytes_sent == sizeof(msg)) {
            metrics_.successful_messages.fetch_add(1);
            return true;
        } else {
            metrics_.failed_messages.fetch_add(1);
            return false;
        }
    }
    
    bool send_ultra_market_data(const std::string& symbol, uint64_t bid_price, uint64_t bid_size,
                                uint64_t ask_price, uint64_t ask_size, uint64_t last_price, uint64_t volume) {
        if (sock_fd_ < 0) return false;
        
        UltraMarketDataMessage msg;
        msg.message_id = message_id_counter_.fetch_add(1);
        msg.update_timestamp();
        strncpy(msg.symbol, symbol.c_str(), 15);
        msg.symbol[15] = '\0';
        msg.bid_price = bid_price;
        msg.bid_size = bid_size;
        msg.ask_price = ask_price;
        msg.ask_size = ask_size;
        msg.last_price = last_price;
        msg.volume = volume;
        
        ssize_t bytes_sent = send(sock_fd_, &msg, sizeof(msg), MSG_DONTWAIT);
        if (bytes_sent == sizeof(msg)) {
            metrics_.successful_messages.fetch_add(1);
            return true;
        } else {
            metrics_.failed_messages.fetch_add(1);
            return false;
        }
    }
    
    // Ultra-low latency performance test
    void run_ultra_latency_test(uint32_t message_count, uint32_t delay_ms = 0) {
        log_performance("Starting Ultra Latency Test");
        log_performance("Target: Sub-10μs latency");
        log_performance("Message Count: " + std::to_string(message_count));
        log_performance("Delay: " + std::to_string(delay_ms) + "ms between messages");
        
        metrics_.reset();
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (uint32_t i = 0; i < message_count && g_running.load(); ++i) {
            auto send_start = std::chrono::high_resolution_clock::now();
            
            // Send order message
            std::string symbol = "SYMBOL" + std::to_string(i % 10);
            uint32_t side = i % 2;
            uint64_t quantity = 100 + i;
            uint64_t price = 1500000 + i;
            
            if (send_ultra_order(symbol, side, quantity, price)) {
                auto send_end = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(send_end - send_start).count();
                metrics_.update_latency(latency);
                
                if (i % 100 == 0) {
                    log_performance("Sent message " + std::to_string(i) + "/" + std::to_string(message_count) + 
                                   " - Latency: " + std::to_string(latency) + "ns");
                }
            }
            
            if (delay_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        print_ultra_performance_results(total_time);
    }
    
    // Ultra-high throughput test
    void run_ultra_throughput_test(uint32_t message_count, uint32_t burst_size = 100) {
        log_performance("Starting Ultra Throughput Test");
        log_performance("Target: Maximum messages per second");
        log_performance("Message Count: " + std::to_string(message_count));
        log_performance("Burst Size: " + std::to_string(burst_size));
        
        metrics_.reset();
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (uint32_t i = 0; i < message_count && g_running.load(); i += burst_size) {
            auto burst_start = std::chrono::high_resolution_clock::now();
            
            // Send burst of messages
            for (uint32_t j = 0; j < burst_size && (i + j) < message_count; ++j) {
                std::string symbol = "SYMBOL" + std::to_string((i + j) % 10);
                uint32_t side = (i + j) % 2;
                uint64_t quantity = 100 + (i + j);
                uint64_t price = 1500000 + (i + j);
                
                send_ultra_order(symbol, side, quantity, price);
            }
            
            auto burst_end = std::chrono::high_resolution_clock::now();
            auto burst_time = std::chrono::duration_cast<std::chrono::nanoseconds>(burst_end - burst_start).count();
            
            if (i % 1000 == 0) {
                log_performance("Sent burst " + std::to_string(i / burst_size) + " - " + 
                               std::to_string(burst_size) + " messages in " + 
                               std::to_string(burst_time / 1000.0) + "μs");
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        print_ultra_performance_results(total_time);
    }
    
    // Ultra stress test
    void run_ultra_stress_test(uint32_t duration_seconds, uint32_t messages_per_second) {
        log_performance("Starting Ultra Stress Test");
        log_performance("Target: Sustained high load for " + std::to_string(duration_seconds) + " seconds");
        log_performance("Load: " + std::to_string(messages_per_second) + " messages/second");
        
        metrics_.reset();
        
        auto start_time = std::chrono::high_resolution_clock::now();
        auto end_time = start_time + std::chrono::seconds(duration_seconds);
        
        uint32_t message_count = 0;
        auto last_report = start_time;
        
        while (std::chrono::high_resolution_clock::now() < end_time && g_running.load()) {
            auto message_start = std::chrono::high_resolution_clock::now();
            
            // Send market data message
            std::string symbol = "SYMBOL" + std::to_string(message_count % 10);
            uint64_t bid_price = 1500000 + (message_count % 1000);
            uint64_t bid_size = 1000 + (message_count % 1000);
            uint64_t ask_price = bid_price + 100;
            uint64_t ask_size = 1000 + (message_count % 1000);
            uint64_t last_price = bid_price + 50;
            uint64_t volume = 10000 + (message_count % 10000);
            
            if (send_ultra_market_data(symbol, bid_price, bid_size, ask_price, ask_size, last_price, volume)) {
                message_count++;
            }
            
            // Rate limiting
            auto message_end = std::chrono::high_resolution_clock::now();
            auto message_time = std::chrono::duration_cast<std::chrono::nanoseconds>(message_end - message_start).count();
            auto target_time = 1000000000ULL / messages_per_second; // nanoseconds per message
            
            if (message_time < target_time) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(target_time - message_time));
            }
            
            // Progress report every second
            auto now = std::chrono::high_resolution_clock::now();
            if (now - last_report >= std::chrono::seconds(1)) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
                log_performance("Stress test progress: " + std::to_string(elapsed) + "s elapsed, " + 
                               std::to_string(message_count) + " messages sent");
                last_report = now;
            }
        }
        
        auto final_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(final_time - start_time).count();
        
        print_ultra_performance_results(total_time);
    }
    
    // Market data streaming test
    void run_market_data_streaming_test(uint32_t duration_seconds, uint32_t updates_per_second) {
        log_performance("Starting Market Data Streaming Test");
        log_performance("Target: Real-time market data streaming for " + std::to_string(duration_seconds) + " seconds");
        log_performance("Update Rate: " + std::to_string(updates_per_second) + " updates/second");
        
        metrics_.reset();
        
        auto start_time = std::chrono::high_resolution_clock::now();
        auto end_time = start_time + std::chrono::seconds(duration_seconds);
        
        uint32_t update_count = 0;
        auto last_report = start_time;
        
        while (std::chrono::high_resolution_clock::now() < end_time && g_running.load()) {
            auto update_start = std::chrono::high_resolution_clock::now();
            
            // Send market data update
            std::string symbol = "SYMBOL" + std::to_string(update_count % 10);
            uint64_t bid_price = 1500000 + (update_count % 1000);
            uint64_t bid_size = 1000 + (update_count % 1000);
            uint64_t ask_price = bid_price + 100;
            uint64_t ask_size = 1000 + (update_count % 1000);
            uint64_t last_price = bid_price + 50;
            uint64_t volume = 10000 + (update_count % 10000);
            
            if (send_ultra_market_data(symbol, bid_price, bid_size, ask_price, ask_size, last_price, volume)) {
                update_count++;
            }
            
            // Rate limiting for streaming
            auto update_end = std::chrono::high_resolution_clock::now();
            auto update_time = std::chrono::duration_cast<std::chrono::nanoseconds>(update_end - update_start).count();
            auto target_time = 1000000000ULL / updates_per_second; // nanoseconds per update
            
            if (update_time < target_time) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(target_time - update_time));
            }
            
            // Progress report every 5 seconds
            auto now = std::chrono::high_resolution_clock::now();
            if (now - last_report >= std::chrono::seconds(5)) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
                log_performance("Streaming progress: " + std::to_string(elapsed) + "s elapsed, " + 
                               std::to_string(update_count) + " updates sent");
                last_report = now;
            }
        }
        
        auto final_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(final_time - start_time).count();
        
        print_ultra_performance_results(total_time);
    }
    
private:
    void print_ultra_performance_results(uint64_t total_time_ms) {
        log_performance("=== Ultra HFT Performance Results ===");
        log_performance("Total Time: " + std::to_string(total_time_ms) + "ms");
        log_performance("Total Messages: " + std::to_string(metrics_.total_messages.load()));
        log_performance("Successful: " + std::to_string(metrics_.successful_messages.load()));
        log_performance("Failed: " + std::to_string(metrics_.failed_messages.load()));
        
        if (metrics_.total_messages.load() > 0) {
            double avg_latency = metrics_.get_average_latency();
            uint64_t min_lat = metrics_.min_latency.load();
            uint64_t max_lat = metrics_.max_latency.load();
            
            log_performance("Average Latency: " + std::to_string(avg_latency / 1000.0) + "μs");
            log_performance("Min Latency: " + std::to_string(min_lat / 1000.0) + "μs");
            log_performance("Max Latency: " + std::to_string(max_lat / 1000.0) + "μs");
            
            // Performance assessment
            if (avg_latency < 10000) { // < 10μs
                log_success("✓ Ultra-low latency target met (< 10μs)");
            } else if (avg_latency < 20000) { // < 20μs
                log_warning("⚠ Latency above ultra target but within standard HFT range");
            } else {
                log_error("✗ Latency above acceptable range");
            }
            
            // Throughput calculation
            double throughput = (metrics_.total_messages.load() * 1000.0) / total_time_ms;
            log_performance("Throughput: " + std::to_string(throughput) + " messages/second");
        }
        
        log_performance("=====================================");
    }
};

// Print usage information
void print_usage(const char* program_name) {
    std::cout << "Ultra HFT Test Client - Specialized for Ultra HFT Server Testing" << std::endl;
    std::cout << "=================================================================" << std::endl;
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --ip <ip>        Server IP address (default: 127.0.0.1)" << std::endl;
    std::cout << "  --port <port>    Server port (default: 8888)" << std::endl;
    std::cout << "  --mode <mode>    Test mode: latency, throughput, stress, streaming (default: latency)" << std::endl;
    std::cout << "  --count <n>      Number of messages for test (default: 1000)" << std::endl;
    std::cout << "  --duration <n>   Test duration in seconds (default: 60)" << std::endl;
    std::cout << "  --rate <n>       Messages per second (default: 1000)" << std::endl;
    std::cout << "  --help           Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Test Modes:" << std::endl;
    std::cout << "  latency         Ultra-low latency test (< 10μs target)" << std::endl;
    std::cout << "  throughput      Maximum throughput test" << std::endl;
    std::cout << "  stress          Sustained high-load stress test" << std::endl;
    std::cout << "  streaming       Real-time market data streaming" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " --mode latency --count 10000" << std::endl;
    std::cout << "  " << program_name << " --mode throughput --count 100000" << std::endl;
    std::cout << "  " << program_name << " --mode stress --duration 300 --rate 5000" << std::endl;
    std::cout << "  " << program_name << " --mode streaming --duration 120 --rate 1000" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    std::string test_mode = "latency";
    uint32_t message_count = 1000;
    uint32_t duration_seconds = 60;
    uint32_t messages_per_second = 1000;
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--ip") == 0) {
            if (i + 1 < argc) server_ip = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) server_port = static_cast<uint16_t>(atoi(argv[++i]));
        } else if (strcmp(argv[i], "--mode") == 0) {
            if (i + 1 < argc) test_mode = argv[++i];
        } else if (strcmp(argv[i], "--count") == 0) {
            if (i + 1 < argc) message_count = static_cast<uint32_t>(atoi(argv[++i]));
        } else if (strcmp(argv[i], "--duration") == 0) {
            if (i + 1 < argc) duration_seconds = static_cast<uint32_t>(atoi(argv[++i]));
        } else if (strcmp(argv[i], "--rate") == 0) {
            if (i + 1 < argc) messages_per_second = static_cast<uint32_t>(atoi(argv[++i]));
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "=== Ultra HFT Test Client ===" << std::endl;
    std::cout << "Server: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Mode: " << test_mode << std::endl;
    std::cout << "========================" << std::endl;
    
    // Create and connect client
    UltraTestClient client(server_ip, server_port);
    
    if (!client.connect()) {
        log_error("Failed to connect to Ultra HFT Server");
        return 1;
    }
    
    // Run selected test mode
    if (test_mode == "latency") {
        client.run_ultra_latency_test(message_count, 1); // 1ms delay for latency measurement
    } else if (test_mode == "throughput") {
        client.run_ultra_throughput_test(message_count, 100);
    } else if (test_mode == "stress") {
        client.run_ultra_stress_test(duration_seconds, messages_per_second);
    } else if (test_mode == "streaming") {
        client.run_market_data_streaming_test(duration_seconds, messages_per_second);
    } else {
        log_error("Unknown test mode: " + test_mode);
        print_usage(argv[0]);
        return 1;
    }
    
    log_success("Ultra HFT test completed successfully");
    return 0;
} 