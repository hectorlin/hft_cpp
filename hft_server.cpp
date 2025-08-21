#include "hft_server.h"
#include <errno.h>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <iomanip>

namespace hft {

// Singleton instance
HFTServer& HFTServer::get_instance() {
    static HFTServer instance;
    return instance;
}

HFTServer::~HFTServer() {
    stop();
}

bool HFTServer::initialize(const std::string& ip, uint16_t port, size_t thread_count) {
    server_ip_ = ip;
    server_port_ = port;
    thread_count_ = thread_count;
    
    // Pre-allocate buffers
    send_buffer_.resize(BUFFER_SIZE);
    recv_buffer_.resize(BUFFER_SIZE);
    
    // Create server socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == -1) {
        std::cerr << "Failed to create server socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set socket options for high performance
    setup_socket_options(server_socket_);
    
    // Bind socket
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    
    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Listen for connections
    if (listen(server_socket_, BACKLOG) == -1) {
        std::cerr << "Failed to listen: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Set non-blocking
    set_non_blocking(server_socket_);
    
    // Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        std::cerr << "Failed to create epoll: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Add server socket to epoll
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.ptr = nullptr;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_socket_, &ev) == -1) {
        std::cerr << "Failed to add server socket to epoll: " << strerror(errno) << std::endl;
        close(epoll_fd_);
        close(server_socket_);
        return false;
    }
    
    std::cout << "HFT Server initialized on " << ip << ":" << port << std::endl;
    return true;
}

void HFTServer::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    
    // Start worker threads (they will handle both accepting and processing)
    for (size_t i = 0; i < thread_count_; ++i) {
        worker_threads_.emplace_back(&HFTServer::worker_thread, this, i);
    }
    
    std::cout << "HFT Server started with " << thread_count_ << " worker threads" << std::endl;
}

void HFTServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Close server socket to unblock accept
    if (server_socket_ != -1) {
        close(server_socket_);
        server_socket_ = -1;
    }
    
    // Join threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Close epoll
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    // Close all client connections
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [fd, conn] : connections_) {
        close(conn->fd);
    }
    connections_.clear();
    
    std::cout << "HFT Server stopped" << std::endl;
}

void HFTServer::accept_connections() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server_socket_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return; // No pending connections
        }
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
        return;
    }
        
        // Set client socket options
        setup_socket_options(client_fd);
        set_non_blocking(client_fd);
        
        // Create connection object
        auto conn = std::make_unique<Connection>();
        conn->fd = client_fd;
        conn->addr = client_addr;
        conn->last_heartbeat = std::chrono::steady_clock::now();
        conn->client_id = reinterpret_cast<uint64_t>(conn.get());
        
        // Add to epoll
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET; // Edge-triggered
        ev.data.ptr = conn.get();
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            std::cerr << "Failed to add client to epoll: " << strerror(errno) << std::endl;
            close(client_fd);
            return;
        }
        
        // Store connection
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[client_fd] = std::move(conn);
            stats_.total_connections++;
            stats_.peak_connections = std::max(stats_.peak_connections, static_cast<uint64_t>(connections_.size()));
        }
        
        std::cout << "New connection from " << inet_ntoa(client_addr.sin_addr) 
                  << ":" << ntohs(client_addr.sin_port) << std::endl;
}

void HFTServer::worker_thread(size_t thread_id) {
    std::vector<epoll_event> events(MAX_EVENTS);
    
    while (running_.load()) {
        int nfds = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, 1); // 1ms timeout
        
        if (nfds == -1) {
            if (errno == EINTR) continue;
            std::cerr << "Worker " << thread_id << " epoll_wait failed: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.ptr == nullptr) {
                // This is the server socket - new connection
                accept_connections();
            } else {
                // This is a client connection
                auto* conn = static_cast<Connection*>(events[i].data.ptr);
                handle_client_events(conn->fd);
            }
        }
    }
}

void HFTServer::handle_client_events(int client_fd) {
    // Find the connection for this file descriptor
    Connection* conn = nullptr;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(client_fd);
        if (it == connections_.end()) {
            return; // Connection not found
        }
        conn = it->second.get();
    }
    
    if (!conn) {
        return;
    }
    
    // Process all available data from the client
    while (true) {
        ssize_t bytes_read = recv(client_fd, recv_buffer_.data(), recv_buffer_.size(), MSG_DONTWAIT);
        
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more data
            }
            std::cerr << "Recv failed: " << strerror(errno) << std::endl;
            return;
        }
        
        if (bytes_read == 0) {
            // Client disconnected
            close_connection(*conn);
            return;
        }
        
        // Process message - check for different message types
        if (bytes_read >= sizeof(Message)) {
            const Message* msg = reinterpret_cast<const Message*>(recv_buffer_.data());
            std::cout << "Processing message type: " << static_cast<int>(msg->message_type) 
                      << " size: " << bytes_read << " bytes" << std::endl;
            
            // Check if this is an OrderMessage (larger than base Message)
            if (msg->message_type == MessageType::ORDER_NEW || 
                msg->message_type == MessageType::ORDER_CANCEL || 
                msg->message_type == MessageType::ORDER_REPLACE) {
                if (bytes_read >= sizeof(OrderMessage)) {
                    const OrderMessage* order_msg = reinterpret_cast<const OrderMessage*>(recv_buffer_.data());
                    process_client_message(*order_msg, *conn);
                } else {
                    std::cout << "Incomplete order message: " << bytes_read << " bytes (need " 
                              << sizeof(OrderMessage) << ")" << std::endl;
                }
            } else if (msg->message_type == MessageType::MARKET_DATA) {
                if (bytes_read >= sizeof(MarketDataMessage)) {
                    const MarketDataMessage* market_msg = reinterpret_cast<const MarketDataMessage*>(recv_buffer_.data());
                    process_client_message(*market_msg, *conn);
                } else {
                    std::cout << "Incomplete market data message: " << bytes_read << " bytes (need " 
                              << sizeof(MarketDataMessage) << ")" << std::endl;
                }
            } else {
                // Regular message
                process_client_message(*msg, *conn);
            }
        } else {
            std::cout << "Received incomplete message: " << bytes_read << " bytes (need at least " 
                      << sizeof(Message) << ")" << std::endl;
        }
    }
}

void HFTServer::process_client_message(const Message& msg, Connection& conn) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "Processing base message type: " << static_cast<int>(msg.message_type) << std::endl;
    
    // Find appropriate service
    std::shared_ptr<IMessageService> service;
    {
        std::lock_guard<std::mutex> lock(services_mutex_);
        auto it = services_.find(msg.message_type);
        if (it != services_.end()) {
            service = it->second;
        }
    }
    
    if (service) {
        service->process_message(msg, conn);
    }
    
    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_messages_processed++;
        
        // Update average latency (exponential moving average)
        constexpr double alpha = 0.01; // Smoothing factor
        double current_latency_us = latency.count() / 1000.0;
        stats_.avg_latency_us = alpha * current_latency_us + (1.0 - alpha) * stats_.avg_latency_us;
    }
}

void HFTServer::process_client_message(const OrderMessage& msg, Connection& conn) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "Processing ORDER message: " << msg.symbol.data() 
              << " " << (msg.side == OrderSide::BUY ? "BUY" : "SELL")
              << " " << msg.quantity << " @ " << msg.price << std::endl;
    
    // Find appropriate service
    std::shared_ptr<IMessageService> service;
    {
        std::lock_guard<std::mutex> lock(services_mutex_);
        auto it = services_.find(msg.message_type);
        if (it != services_.end()) {
            service = it->second;
        }
    }
    
    if (service) {
        service->process_message(msg, conn);
    }
    
    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_messages_processed++;
        
        // Update average latency (exponential moving average)
        constexpr double alpha = 0.01; // Smoothing factor
        double current_latency_us = latency.count() / 1000.0;
        stats_.avg_latency_us = alpha * current_latency_us + (1.0 - alpha) * stats_.avg_latency_us;
    }
}

void HFTServer::process_client_message(const MarketDataMessage& msg, Connection& conn) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "Processing MARKET_DATA message: " << msg.symbol.data() 
              << " Bid: " << msg.bid_price << " Ask: " << msg.ask_price << std::endl;
    
    // Find appropriate service
    std::shared_ptr<IMessageService> service;
    {
        std::lock_guard<std::mutex> lock(services_mutex_);
        auto it = services_.find(msg.message_type);
        if (it != services_.end()) {
            service = it->second;
        }
    }
    
    if (service) {
        service->process_message(msg, conn);
    }
    
    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_messages_processed++;
        
        // Update average latency (exponential moving average)
        constexpr double alpha = 0.01; // Smoothing factor
        double current_latency_us = latency.count() / 1000.0;
        stats_.avg_latency_us = alpha * current_latency_us + (1.0 - alpha) * stats_.avg_latency_us;
    }
}

void HFTServer::send_response(Connection& conn, const Message& response) {
    // Zero-copy send using pre-allocated buffer
    memcpy(send_buffer_.data(), &response, sizeof(Message));
    
    ssize_t bytes_sent = send(conn.fd, send_buffer_.data(), sizeof(Message), MSG_NOSIGNAL);
    if (bytes_sent == -1) {
        std::cerr << "Send failed: " << strerror(errno) << std::endl;
    }
}

void HFTServer::close_connection(Connection& conn) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, conn.fd, nullptr);
    close(conn.fd);
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.erase(conn.fd);
    }
}

void HFTServer::setup_socket_options(int sock_fd) {
    // Set SO_REUSEADDR
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set TCP_NODELAY for low latency
    setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    
    // Set SO_KEEPALIVE
    setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    
    // Set send and receive buffer sizes
    int send_buf_size = 1024 * 1024; // 1MB
    int recv_buf_size = 1024 * 1024; // 1MB
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
}

void HFTServer::set_non_blocking(int sock_fd) {
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
}

HFTServer::ServerStats HFTServer::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void HFTServer::register_service(MessageType type, std::shared_ptr<IMessageService> service) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    services_[type] = service;
}

// OrderService implementation
void OrderService::process_message(const Message& msg, Connection& conn) {
    switch (msg.message_type) {
        case MessageType::ORDER_NEW:
            if (msg.payload_size >= sizeof(OrderMessage)) {
                const OrderMessage& order = *reinterpret_cast<const OrderMessage*>(&msg);
                handle_new_order(order, conn);
            }
            break;
        case MessageType::ORDER_CANCEL:
            handle_cancel_order(msg, conn);
            break;
        case MessageType::ORDER_REPLACE:
            handle_replace_order(msg, conn);
            break;
        default:
            break;
    }
}

void OrderService::on_connection_established(Connection& conn) {
    conn.is_authenticated = true;
}

void OrderService::on_connection_closed(Connection& conn) {
    conn.is_authenticated = false;
}

void OrderService::handle_new_order(const OrderMessage& order, Connection& conn) {
    // Process new order (implement order matching logic here)
    // For now, just send confirmation
    Message response = order;
    response.status = MessageStatus::PROCESSED;
    response.update_timestamp();
    
    // In real implementation, you'd send this back to the client
    // For now, just log it
    std::cout << "New order received: " << order.symbol.data() 
              << " " << (order.side == OrderSide::BUY ? "BUY" : "SELL")
              << " " << order.quantity << " @ " << order.price << std::endl;
}

void OrderService::handle_cancel_order(const Message& msg, Connection& conn) {
    // Handle order cancellation
    std::cout << "Cancel order received" << std::endl;
}

void OrderService::handle_replace_order(const Message& msg, Connection& conn) {
    // Handle order replacement
    std::cout << "Replace order received" << std::endl;
}

// MarketDataService implementation
void MarketDataService::process_message(const Message& msg, Connection& conn) {
    if (msg.message_type == MessageType::MARKET_DATA) {
        if (msg.payload_size >= sizeof(MarketDataMessage)) {
            const MarketDataMessage& data = *reinterpret_cast<const MarketDataMessage*>(&msg);
            broadcast_market_data(data);
        }
    }
}

void MarketDataService::on_connection_established(Connection& conn) {
    // Send initial market data snapshot
    std::cout << "Market data connection established" << std::endl;
}

void MarketDataService::on_connection_closed(Connection& conn) {
    std::cout << "Market data connection closed" << std::endl;
}

void MarketDataService::broadcast_market_data(const MarketDataMessage& data) {
    // Broadcast market data to all connected clients
    // In real implementation, you'd maintain a list of market data subscribers
    std::cout << "Broadcasting market data for " << data.symbol.data() << std::endl;
}

} // namespace hft 