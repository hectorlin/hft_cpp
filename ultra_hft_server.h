#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <memory>
#include <functional>

namespace ultra_hft {

// Lock-free ring buffer for ultra-low latency
template<typename T, size_t SIZE>
class LockFreeRingBuffer {
    static_assert(SIZE > 0 && ((SIZE & (SIZE - 1)) == 0), "Size must be a power of 2");
    
private:
    std::array<T, SIZE> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    static constexpr size_t MASK = SIZE - 1;
    
public:
    LockFreeRingBuffer() = default;
    
    bool push(const T& item) noexcept {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) & MASK;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) noexcept {
        size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) & MASK, std::memory_order_release);
        return true;
    }
    
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
    
    bool full() const noexcept {
        size_t next_tail = (tail_.load(std::memory_order_relaxed) + 1) & MASK;
        return next_tail == head_.load(std::memory_order_acquire);
    }
    
    size_t size() const noexcept {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return (tail - head) & MASK;
    }
    
    size_t capacity() const noexcept {
        return SIZE - 1; // Leave one slot empty for full detection
    }
};

// Ultra-optimized message structure (cache-line aligned)
struct alignas(64) UltraMessage {
    uint64_t message_id;
    uint64_t timestamp;
    uint32_t message_type;
    uint32_t payload_size;
    std::array<uint8_t, 1024> payload;
    
    UltraMessage() : message_id(0), timestamp(0), message_type(0), payload_size(0) {}
    
    void update_timestamp() {
        timestamp = get_current_timestamp();
    }
    
    static uint64_t get_current_timestamp() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    }
};

// Ultra-optimized order message
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

// Ultra-optimized market data message
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

// Ultra-optimized connection structure
struct alignas(64) UltraConnection {
    int fd;
    struct sockaddr_in addr;
    uint64_t last_heartbeat;
    uint64_t client_id;
    std::atomic<bool> is_authenticated{false};
    std::atomic<bool> is_active{true};
    
    UltraConnection() : fd(-1), last_heartbeat(0), client_id(0) {}
};

// Ultra-optimized server statistics
struct alignas(64) UltraServerStats {
    std::atomic<uint64_t> total_messages{0};
    std::atomic<uint64_t> active_connections{0};
    std::atomic<uint64_t> peak_connections{0};
    std::atomic<uint64_t> total_latency{0};
    std::atomic<uint64_t> message_count{0};
    
    double get_average_latency() const {
        uint64_t count = message_count.load();
        if (count == 0) return 0.0;
        return static_cast<double>(total_latency.load()) / count;
    }
};

// Ultra-optimized HFT server class
class UltraHFTServer {
private:
    // Server configuration
    std::string server_ip_;
    uint16_t server_port_;
    uint32_t thread_count_;
    
    // Server state
    std::atomic<bool> running_{false};
    int server_socket_;
    int epoll_fd_;
    
    // Lock-free queues for message processing
    static constexpr size_t QUEUE_SIZE = 65536; // Power of 2 for efficient masking
    LockFreeRingBuffer<UltraMessage*, QUEUE_SIZE> message_queue_;
    LockFreeRingBuffer<UltraConnection*, QUEUE_SIZE> connection_queue_;
    
    // Worker threads
    std::vector<std::thread> worker_threads_;
    
    // Connections (lock-free access)
    std::vector<std::unique_ptr<UltraConnection>> connections_;
    std::atomic<size_t> connection_count_{0};
    
    // Statistics
    UltraServerStats stats_;
    
    // Pre-allocated buffers for zero-copy operations
    std::array<UltraMessage, 1024> send_buffer_;
    std::array<UltraMessage, 1024> recv_buffer_;
    std::atomic<size_t> send_buffer_index_{0};
    std::atomic<size_t> recv_buffer_index_{0};
    
    // Performance monitoring
    std::atomic<uint64_t> last_stats_time_{0};
    
public:
    UltraHFTServer(const std::string& ip = "127.0.0.1", uint16_t port = 8888, uint32_t threads = 4)
        : server_ip_(ip), server_port_(port), thread_count_(threads) {}
    
    ~UltraHFTServer() {
        stop();
    }
    
    // Singleton pattern
    static UltraHFTServer& get_instance() {
        static UltraHFTServer instance;
        return instance;
    }
    
    // Initialize server
    bool initialize();
    
    // Start server
    bool start();
    
    // Stop server
    void stop();
    
    // Get statistics (return reference to avoid atomic copy issues)
    const UltraServerStats& get_stats() const { return stats_; }
    
    // Process message (lock-free)
    void process_message(UltraMessage* msg, UltraConnection* conn);
    
private:
    // Worker thread function
    void worker_thread();
    
    // Accept new connections
    void accept_connections();
    
    // Handle client events
    void handle_client_events(int client_fd);
    
    // Send response (zero-copy)
    bool send_response(UltraConnection* conn, const UltraMessage* msg);
    
    // Close connection
    void close_connection(UltraConnection* conn);
    
    // Setup socket options for ultra-low latency
    bool setup_socket_options(int sock);
    
    // Set non-blocking mode
    bool set_non_blocking(int sock);
    
    // Get next buffer slot (lock-free)
    UltraMessage* get_next_send_buffer();
    UltraMessage* get_next_recv_buffer();
    
    // Process specific message types
    void process_order_message(UltraOrderMessage* msg, UltraConnection* conn);
    void process_market_data_message(UltraMarketDataMessage* msg, UltraConnection* conn);
    
    // Process message queue (lock-free)
    void process_message_queue();
    
    // Performance monitoring
    void update_stats(uint64_t latency);
    void print_stats();
};

} // namespace ultra_hft 