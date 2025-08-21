#ifndef HFT_SERVER_H
#define HFT_SERVER_H

#include "message.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace hft {

/**
 * @brief Connection information structure
 */
struct Connection {
    int fd;                         // File descriptor
    sockaddr_in addr;               // Client address
    std::chrono::steady_clock::time_point last_heartbeat;
    uint64_t client_id;
    bool is_authenticated;
    
    Connection() : fd(-1), client_id(0), is_authenticated(false) {
        memset(&addr, 0, sizeof(addr));
    }
};

/**
 * @brief Service interface for message processing
 */
class IMessageService {
public:
    virtual ~IMessageService() = default;
    virtual void process_message(const Message& msg, Connection& conn) = 0;
    virtual void on_connection_established(Connection& conn) = 0;
    virtual void on_connection_closed(Connection& conn) = 0;
};

/**
 * @brief Order management service
 */
class OrderService : public IMessageService {
public:
    void process_message(const Message& msg, Connection& conn) override;
    void on_connection_established(Connection& conn) override;
    void on_connection_closed(Connection& conn) override;
    
private:
    void handle_new_order(const OrderMessage& order, Connection& conn);
    void handle_cancel_order(const Message& msg, Connection& conn);
    void handle_replace_order(const Message& msg, Connection& conn);
};

/**
 * @brief Market data service
 */
class MarketDataService : public IMessageService {
public:
    void process_message(const Message& msg, Connection& conn) override;
    void on_connection_established(Connection& conn) override;
    void on_connection_closed(Connection& conn) override;
    
private:
    void broadcast_market_data(const MarketDataMessage& data);
};

/**
 * @brief Main HFT server class (Singleton)
 */
class HFTServer {
public:
    static HFTServer& get_instance();
    
    // Delete copy constructor and assignment operator
    HFTServer(const HFTServer&) = delete;
    HFTServer& operator=(const HFTServer&) = delete;
    
    /**
     * @brief Initialize the server
     */
    bool initialize(const std::string& ip, uint16_t port, size_t thread_count = 4);
    
    /**
     * @brief Start the server
     */
    void start();
    
    /**
     * @brief Stop the server
     */
    void stop();
    
    /**
     * @brief Get server statistics
     */
    struct ServerStats {
        uint64_t total_messages_processed;
        uint64_t total_connections;
        double avg_latency_us;
        uint64_t peak_connections;
    };
    
    ServerStats get_stats() const;
    
    /**
     * @brief Register a message service
     */
    void register_service(MessageType type, std::shared_ptr<IMessageService> service);
    
private:
    HFTServer() = default;
    ~HFTServer();
    
    void worker_thread(size_t thread_id);
    void accept_connections();
    void handle_client_events(int client_fd);
    void process_client_message(const Message& msg, Connection& conn);
    void process_client_message(const OrderMessage& msg, Connection& conn);
    void process_client_message(const MarketDataMessage& msg, Connection& conn);
    void send_response(Connection& conn, const Message& response);
    void close_connection(Connection& conn);
    void setup_socket_options(int sock_fd);
    void set_non_blocking(int sock_fd);
    
    // Server configuration
    std::string server_ip_;
    uint16_t server_port_;
    size_t thread_count_;
    
    // Server state
    std::atomic<bool> running_{false};
    int server_socket_{-1};
    int epoll_fd_{-1};
    
    // Threading
    std::vector<std::thread> worker_threads_;
    
    // Connections
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    mutable std::mutex connections_mutex_;
    
    // Services
    std::unordered_map<MessageType, std::shared_ptr<IMessageService>> services_;
    mutable std::mutex services_mutex_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ServerStats stats_{};
    
    // Performance optimization
    static constexpr size_t MAX_EVENTS = 1024;
    static constexpr size_t BUFFER_SIZE = 4096;
    static constexpr int BACKLOG = 1024;
    
    // Pre-allocated buffers for zero-copy operations
    std::vector<uint8_t> send_buffer_;
    std::vector<uint8_t> recv_buffer_;
};

} // namespace hft

#endif // HFT_SERVER_H 