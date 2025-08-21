#include "ultra_hft_server.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cassert>

namespace ultra_hft {

// Ultra-optimized HFT server implementation
bool UltraHFTServer::initialize() {
    std::cout << "=== Ultra HFT Server Initializing ===" << std::endl;
    std::cout << "Server IP: " << server_ip_ << std::endl;
    std::cout << "Server Port: " << server_port_ << std::endl;
    std::cout << "Worker Threads: " << thread_count_ << std::endl;
    std::cout << "Target Latency: < 10μs" << std::endl;
    std::cout << "================================" << std::endl;
    
    // Create server socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Setup socket options for ultra-low latency
    if (!setup_socket_options(server_socket_)) {
        std::cerr << "Failed to setup socket options" << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Set non-blocking mode
    if (!set_non_blocking(server_socket_)) {
        std::cerr << "Failed to set non-blocking mode" << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip_.c_str());
    server_addr.sin_port = htons(server_port_);
    
    if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Listen for connections
    if (listen(server_socket_, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        std::cerr << "Failed to create epoll: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Add server socket to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = nullptr; // Server socket marker
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_socket_, &ev) < 0) {
        std::cerr << "Failed to add server socket to epoll: " << strerror(errno) << std::endl;
        close(epoll_fd_);
        close(server_socket_);
        return false;
    }
    
    std::cout << "Ultra HFT Server initialized on " << server_ip_ << ":" << server_port_ << std::endl;
    return true;
}

bool UltraHFTServer::start() {
    if (running_.load()) {
        std::cerr << "Server already running" << std::endl;
        return false;
    }
    
    running_.store(true);
    std::cout << "Ultra HFT Server starting with " << thread_count_ << " worker threads" << std::endl;
    
    // Start worker threads
    for (uint32_t i = 0; i < thread_count_; ++i) {
        worker_threads_.emplace_back(&UltraHFTServer::worker_thread, this);
    }
    
    std::cout << "Ultra HFT Server started successfully" << std::endl;
    return true;
}

void UltraHFTServer::stop() {
    if (!running_.load()) return;
    
    std::cout << "Stopping Ultra HFT Server..." << std::endl;
    running_.store(false);
    
    // Join worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Close sockets
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
    
    std::cout << "Ultra HFT Server stopped" << std::endl;
}

void UltraHFTServer::worker_thread() {
    constexpr int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
    
    while (running_.load()) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait failed: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.ptr == nullptr) {
                // Server socket event - accept new connections
                accept_connections();
            } else {
                // Client socket event
                UltraConnection* conn = static_cast<UltraConnection*>(events[i].data.ptr);
                if (conn && conn->is_active.load()) {
                    handle_client_events(conn->fd);
                }
            }
        }
        
        // Process message queue
        process_message_queue();
        
        // Print stats periodically
        static uint64_t last_print = 0;
        uint64_t now = UltraMessage::get_current_timestamp();
        if (now - last_print > 1000000000) { // Every second
            print_stats();
            last_print = now;
        }
    }
}

void UltraHFTServer::accept_connections() {
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_socket_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more connections to accept
            }
            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
            break;
        }
        
        // Setup client socket for ultra-low latency
        if (!setup_socket_options(client_fd) || !set_non_blocking(client_fd)) {
            close(client_fd);
            continue;
        }
        
        // Create new connection
        auto conn = std::make_unique<UltraConnection>();
        conn->fd = client_fd;
        conn->addr = client_addr;
        conn->last_heartbeat = UltraMessage::get_current_timestamp();
        conn->client_id = connection_count_.fetch_add(1);
        conn->is_authenticated.store(true);
        
        // Add to epoll
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = conn.get();
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            std::cerr << "Failed to add client to epoll: " << strerror(errno) << std::endl;
            close(client_fd);
            continue;
        }
        
        // Store connection
        connections_.push_back(std::move(conn));
        
        // Update stats
        uint64_t active = stats_.active_connections.fetch_add(1) + 1;
        uint64_t peak = stats_.peak_connections.load();
        while (active > peak && !stats_.peak_connections.compare_exchange_weak(peak, active)) {}
        
        std::cout << "New connection accepted: " << inet_ntoa(client_addr.sin_addr) 
                  << ":" << ntohs(client_addr.sin_port) << " (ID: " << conn->client_id << ")" << std::endl;
    }
}

void UltraHFTServer::handle_client_events(int client_fd) {
    // Find connection
    UltraConnection* conn = nullptr;
    for (auto& c : connections_) {
        if (c->fd == client_fd) {
            conn = c.get();
            break;
        }
    }
    
    if (!conn) return;
    
    // Get receive buffer
    UltraMessage* msg = get_next_recv_buffer();
    if (!msg) return;
    
    // Receive data
    ssize_t bytes_read = recv(client_fd, msg, sizeof(UltraMessage), MSG_DONTWAIT);
    if (bytes_read <= 0) {
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return; // No data available
        }
        close_connection(conn);
        return;
    }
    
    if (bytes_read < sizeof(UltraMessage)) {
        return; // Incomplete message
    }
    
    // Update timestamp for latency measurement
    uint64_t receive_time = UltraMessage::get_current_timestamp();
    uint64_t latency = receive_time - msg->timestamp;
    
    // Update statistics
    update_stats(latency);
    
    // Process message
    process_message(msg, conn);
}

void UltraHFTServer::process_message(UltraMessage* msg, UltraConnection* conn) {
    if (!msg || !conn) return;
    
    // Dispatch based on message type
    switch (msg->message_type) {
        case 1: // ORDER_NEW
            process_order_message(reinterpret_cast<UltraOrderMessage*>(msg), conn);
            break;
        case 2: // MARKET_DATA
            process_market_data_message(reinterpret_cast<UltraMarketDataMessage*>(msg), conn);
            break;
        default:
            std::cout << "Unknown message type: " << msg->message_type << std::endl;
            break;
    }
    
    // Update message count
    stats_.total_messages.fetch_add(1);
}

void UltraHFTServer::process_order_message(UltraOrderMessage* msg, UltraConnection* conn) {
    if (!msg || !conn) return;
    
    std::cout << "Processing ORDER: " << msg->symbol 
              << " " << (msg->side == 0 ? "BUY" : "SELL")
              << " " << msg->quantity << " @ " << msg->price
              << " (ID: " << msg->message_id << ")" << std::endl;
    
    // Send acknowledgment (zero-copy)
    UltraMessage* response = get_next_send_buffer();
    if (response) {
        response->message_id = msg->message_id;
        response->timestamp = UltraMessage::get_current_timestamp();
        response->message_type = 3; // ORDER_ACK
        
        send_response(conn, response);
    }
}

void UltraHFTServer::process_market_data_message(UltraMarketDataMessage* msg, UltraConnection* conn) {
    if (!msg || !conn) return;
    
    std::cout << "Processing MARKET_DATA: " << msg->symbol
              << " Bid: " << msg->bid_price << "x" << msg->bid_size
              << " Ask: " << msg->ask_price << "x" << msg->ask_size
              << " (ID: " << msg->message_id << ")" << std::endl;
    
    // Send acknowledgment (zero-copy)
    UltraMessage* response = get_next_send_buffer();
    if (response) {
        response->message_id = msg->message_id;
        response->timestamp = UltraMessage::get_current_timestamp();
        response->message_type = 4; // MARKET_DATA_ACK
        
        send_response(conn, response);
    }
}

void UltraHFTServer::process_message_queue() {
    UltraMessage* msg;
    UltraConnection* conn;
    
    // Process message queue
    while (message_queue_.pop(msg)) {
        // Process message (implement your business logic here)
        // For now, just update stats
        stats_.total_messages.fetch_add(1);
    }
    
    // Process connection queue
    while (connection_queue_.pop(conn)) {
        // Process connection events
        // For now, just update stats
    }
}

bool UltraHFTServer::send_response(UltraConnection* conn, const UltraMessage* msg) {
    if (!conn || !msg || !conn->is_active.load()) return false;
    
    ssize_t bytes_sent = send(conn->fd, msg, sizeof(UltraMessage), MSG_DONTWAIT);
    return bytes_sent == sizeof(UltraMessage);
}

void UltraHFTServer::close_connection(UltraConnection* conn) {
    if (!conn) return;
    
    conn->is_active.store(false);
    
    // Remove from epoll
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, conn->fd, nullptr);
    
    // Close socket
    close(conn->fd);
    
    // Update stats
    stats_.active_connections.fetch_sub(1);
    
    std::cout << "Connection closed: " << inet_ntoa(conn->addr.sin_addr) 
              << ":" << ntohs(conn->addr.sin_port) << std::endl;
}

bool UltraHFTServer::setup_socket_options(int sock) {
    int opt = 1;
    
    // SO_REUSEADDR
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        return false;
    }
    
    // TCP_NODELAY
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        return false;
    }
    
    // SO_KEEPALIVE
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        return false;
    }
    
    // Large send/receive buffers
    int send_buf_size = 1024 * 1024; // 1MB
    int recv_buf_size = 1024 * 1024; // 1MB
    
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size)) < 0) {
        return false;
    }
    
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size)) < 0) {
        return false;
    }
    
    return true;
}

bool UltraHFTServer::set_non_blocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return false;
    
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
}

UltraMessage* UltraHFTServer::get_next_send_buffer() {
    size_t index = send_buffer_index_.fetch_add(1) % send_buffer_.size();
    return &send_buffer_[index];
}

UltraMessage* UltraHFTServer::get_next_recv_buffer() {
    size_t index = recv_buffer_index_.fetch_add(1) % recv_buffer_.size();
    return &recv_buffer_[index];
}

void UltraHFTServer::update_stats(uint64_t latency) {
    stats_.total_latency.fetch_add(latency);
    stats_.message_count.fetch_add(1);
}

void UltraHFTServer::print_stats() {
    const UltraServerStats& current_stats = get_stats();
    
    std::cout << "=== Ultra HFT Server Statistics ===" << std::endl;
    std::cout << "Total Messages: " << current_stats.total_messages.load() << std::endl;
    std::cout << "Active Connections: " << current_stats.active_connections.load() << std::endl;
    std::cout << "Peak Connections: " << current_stats.peak_connections.load() << std::endl;
    
    double avg_latency = current_stats.get_average_latency();
    std::cout << "Average Latency: " << std::fixed << std::setprecision(2) 
              << (avg_latency / 1000.0) << " μs" << std::endl;
    
    if (avg_latency < 10000) { // < 10μs
        std::cout << "✓ Ultra-low latency target met (< 10μs)" << std::endl;
    } else {
        std::cout << "⚠ Latency above target" << std::endl;
    }
    
    std::cout << "=================================" << std::endl;
}

} // namespace ultra_hft 