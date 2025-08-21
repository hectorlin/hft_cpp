#include "message.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <vector>
#include <atomic>
#include <csignal>

using namespace hft;

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
        g_running.store(false);
    }
}

class TestClient {
public:
    TestClient(const std::string& ip, uint16_t port) 
        : server_ip_(ip), server_port_(port), socket_fd_(-1), message_counter_(0) {
        
        // Set up signal handling
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
    }
    
    ~TestClient() {
        disconnect();
    }
    
    bool connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ == -1) {
            log_error("Failed to create socket: " + std::string(strerror(errno)));
            return false;
        }
        
        // Set socket options for low latency
        int opt = 1;
        setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        server_addr.sin_addr.s_addr = inet_addr(server_ip_.c_str());
        
        if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
            log_error("Failed to connect: " + std::string(strerror(errno)));
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        log_success("Connected to server at " + server_ip_ + ":" + std::to_string(server_port_));
        return true;
    }
    
    void disconnect() {
        if (socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
            log_info("Disconnected from server");
        }
    }
    
    bool send_order(const std::string& symbol, OrderSide side, uint32_t quantity, uint64_t price) {
        if (socket_fd_ == -1) {
            log_error("Not connected to server");
            return false;
        }
        
        OrderMessage order;
        order.message_id = generate_message_id();
        order.update_timestamp();
        order.message_type = MessageType::ORDER_NEW;
        order.status = MessageStatus::PENDING;
        order.source_id = 1;
        order.destination_id = 0;
        
        // Set order details
        strncpy(order.symbol.data(), symbol.c_str(), order.symbol.size() - 1);
        order.side = side;
        order.order_type = OrderType::LIMIT;
        order.time_in_force = TimeInForce::DAY;
        order.order_id = generate_order_id();
        order.client_order_id = order.order_id;
        order.quantity = quantity;
        order.price = price;
        order.stop_price = 0;
        
        // Log order details
        std::stringstream ss;
        ss << "Sending ORDER_NEW: " << symbol << " " 
           << (side == OrderSide::BUY ? "BUY" : "SELL") 
           << " " << quantity << " @ " << price 
           << " (ID: " << order.order_id << ")";
        log_info(ss.str());
        
        // Send order
        ssize_t bytes_sent = send(socket_fd_, &order, sizeof(order), 0);
        if (bytes_sent == -1) {
            log_error("Failed to send order: " + std::string(strerror(errno)));
            return false;
        }
        
        log_success("Order sent successfully (" + std::to_string(bytes_sent) + " bytes)");
        return true;
    }
    
    bool send_cancel_order(uint64_t order_id) {
        if (socket_fd_ == -1) {
            log_error("Not connected to server");
            return false;
        }
        
        Message cancel_msg;
        cancel_msg.message_id = generate_message_id();
        cancel_msg.update_timestamp();
        cancel_msg.message_type = MessageType::ORDER_CANCEL;
        cancel_msg.status = MessageStatus::PENDING;
        cancel_msg.source_id = 1;
        cancel_msg.destination_id = 0;
        
        // Store order_id in payload
        memcpy(cancel_msg.payload.data(), &order_id, sizeof(order_id));
        cancel_msg.payload_size = sizeof(order_id);
        
        log_info("Sending ORDER_CANCEL for order ID: " + std::to_string(order_id));
        
        ssize_t bytes_sent = send(socket_fd_, &cancel_msg, sizeof(cancel_msg), 0);
        if (bytes_sent == -1) {
            log_error("Failed to send cancel order: " + std::string(strerror(errno)));
            return false;
        }
        
        log_success("Cancel order sent successfully (" + std::to_string(bytes_sent) + " bytes)");
        return true;
    }
    
    bool send_market_data(const std::string& symbol, uint64_t bid_price, uint32_t bid_size,
                         uint64_t ask_price, uint32_t ask_size) {
        if (socket_fd_ == -1) {
            log_error("Not connected to server");
            return false;
        }
        
        MarketDataMessage data;
        data.message_id = generate_message_id();
        data.update_timestamp();
        data.message_type = MessageType::MARKET_DATA;
        data.status = MessageStatus::PENDING;
        data.source_id = 2;
        data.destination_id = 0;
        
        // Set market data
        strncpy(data.symbol.data(), symbol.c_str(), data.symbol.size() - 1);
        data.bid_price = bid_price;
        data.bid_size = bid_size;
        data.ask_price = ask_price;
        data.ask_size = ask_size;
        data.last_price = (bid_price + ask_price) / 2;
        data.last_size = 100;
        data.volume = 1000000;
        data.high_price = ask_price + 100;
        data.low_price = bid_price - 100;
        
        std::stringstream ss;
        ss << "Sending MARKET_DATA: " << symbol 
           << " Bid: " << bid_price << "x" << bid_size
           << " Ask: " << ask_price << "x" << ask_size;
        log_info(ss.str());
        
        ssize_t bytes_sent = send(socket_fd_, &data, sizeof(data), 0);
        if (bytes_sent == -1) {
            log_error("Failed to send market data: " + std::string(strerror(errno)));
            return false;
        }
        
        log_success("Market data sent successfully (" + std::to_string(bytes_sent) + " bytes)");
        return true;
    }
    
    bool send_heartbeat() {
        if (socket_fd_ == -1) {
            log_error("Not connected to server");
            return false;
        }
        
        Message heartbeat;
        heartbeat.message_id = generate_message_id();
        heartbeat.update_timestamp();
        heartbeat.message_type = MessageType::HEARTBEAT;
        heartbeat.status = MessageStatus::PENDING;
        heartbeat.source_id = 1;
        heartbeat.destination_id = 0;
        heartbeat.payload_size = 0;
        
        log_info("Sending HEARTBEAT");
        
        ssize_t bytes_sent = send(socket_fd_, &heartbeat, sizeof(heartbeat), 0);
        if (bytes_sent == -1) {
            log_error("Failed to send heartbeat: " + std::string(strerror(errno)));
            return false;
        }
        
        return true;
    }
    
    void run_performance_test(int num_orders, int delay_ms = 100) {
        log_info("Starting performance test with " + std::to_string(num_orders) + " orders");
        log_info("Delay between orders: " + std::to_string(delay_ms) + "ms");
        
        auto start_time = std::chrono::high_resolution_clock::now();
        int success_count = 0;
        int failure_count = 0;
        
        std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN"};
        
        for (int i = 0; i < num_orders && g_running.load(); ++i) {
            std::string symbol = symbols[i % symbols.size()];
            OrderSide side = (i % 2 == 0) ? OrderSide::BUY : OrderSide::SELL;
            uint32_t quantity = 100 + (i % 1000);
            uint64_t price = 1500000 + (i % 10000); // Price in ticks
            
            if (send_order(symbol, side, quantity, price)) {
                success_count++;
            } else {
                failure_count++;
            }
            
            // Small delay to avoid overwhelming the server
            if (delay_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        log_success("Performance test completed in " + std::to_string(duration.count()) + "ms");
        log_info("Results: " + std::to_string(success_count) + " successful, " + 
                std::to_string(failure_count) + " failed");
        
        if (duration.count() > 0) {
            double throughput = (success_count * 1000.0) / duration.count();
            std::stringstream ss;
            ss << "Average throughput: " << std::fixed << std::setprecision(2) << throughput << " orders/sec";
            log_info(ss.str());
        }
    }
    
    void run_market_data_test(int num_updates, int delay_ms = 200) {
        log_info("Starting market data test with " + std::to_string(num_updates) + " updates");
        
        std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN"};
        std::vector<uint64_t> base_prices = {1500000, 2800000, 400000, 250000, 3500000};
        
        for (int i = 0; i < num_updates && g_running.load(); ++i) {
            int symbol_idx = i % symbols.size();
            std::string symbol = symbols[symbol_idx];
            uint64_t base_price = base_prices[symbol_idx];
            
            // Simulate price movement
            uint64_t bid_price = base_price + (i * 10) % 1000;
            uint64_t ask_price = bid_price + 100 + (i * 5) % 200;
            uint32_t bid_size = 1000 + (i * 100) % 5000;
            uint32_t ask_size = 1000 + (i * 150) % 5000;
            
            if (send_market_data(symbol, bid_price, bid_size, ask_price, ask_size)) {
                log_success("Market data update " + std::to_string(i + 1) + "/" + 
                           std::to_string(num_updates) + " sent");
            }
            
            if (delay_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
        }
        
        log_success("Market data test completed");
    }
    
    void run_heartbeat_test(int duration_seconds = 30) {
        log_info("Starting heartbeat test for " + std::to_string(duration_seconds) + " seconds");
        
        auto start_time = std::chrono::steady_clock::now();
        int heartbeat_count = 0;
        
        while (g_running.load()) {
            auto now = std::chrono::steady_clock::now();
            if (now - start_time > std::chrono::seconds(duration_seconds)) {
                break;
            }
            
            if (send_heartbeat()) {
                heartbeat_count++;
                log_info("Heartbeat " + std::to_string(heartbeat_count) + " sent");
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        log_success("Heartbeat test completed. Sent " + std::to_string(heartbeat_count) + " heartbeats");
    }
    
    void run_comprehensive_test() {
        log_info("Starting comprehensive test suite");
        log_info("=================================");
        
        // Test 1: Basic connectivity
        log_info("Test 1: Basic connectivity");
        if (!connect()) {
            log_error("Basic connectivity test failed");
            return;
        }
        
        // Test 2: Market data
        log_info("Test 2: Market data transmission");
        send_market_data("AAPL", 1500000, 1000, 1500100, 1000);
        send_market_data("GOOGL", 2800000, 500, 2800100, 500);
        
        // Test 3: Order management
        log_info("Test 3: Order management");
        send_order("AAPL", OrderSide::BUY, 100, 1500000);
        send_order("GOOGL", OrderSide::SELL, 50, 2800100);
        
        // Test 4: Performance test
        log_info("Test 4: Performance test");
        run_performance_test(100, 50);
        
        // Test 5: Market data streaming
        log_info("Test 5: Market data streaming");
        run_market_data_test(50, 100);
        
        // Test 6: Heartbeat
        log_info("Test 6: Heartbeat test");
        run_heartbeat_test(10);
        
        log_success("Comprehensive test suite completed successfully");
    }
    
    void interactive_mode() {
        log_info("Entering interactive mode. Commands:");
        log_info("  order <symbol> <side> <quantity> <price>  - Send order");
        log_info("  cancel <order_id>                          - Cancel order");
        log_info("  market <symbol> <bid> <ask>               - Send market data");
        log_info("  heartbeat                                  - Send heartbeat");
        log_info("  quit                                       - Exit");
        log_info("  help                                       - Show this help");
        
        std::string command;
        while (g_running.load()) {
            std::cout << "\nHFT> ";
            std::getline(std::cin, command);
            
            if (command == "quit" || command == "exit") {
                break;
            } else if (command == "help") {
                log_info("Available commands:");
                log_info("  order <symbol> <side> <quantity> <price>  - Send order");
                log_info("  cancel <order_id>                          - Cancel order");
                log_info("  market <symbol> <bid> <ask>               - Send market data");
                log_info("  heartbeat                                  - Send heartbeat");
                log_info("  quit                                       - Exit");
                log_info("  help                                       - Show this help");
            } else if (command.substr(0, 5) == "order") {
                std::stringstream ss(command.substr(6));
                std::string symbol, side_str;
                uint32_t quantity;
                uint64_t price;
                
                if (ss >> symbol >> side_str >> quantity >> price) {
                    OrderSide side = (side_str == "buy" || side_str == "BUY") ? OrderSide::BUY : OrderSide::SELL;
                    send_order(symbol, side, quantity, price);
                } else {
                    log_error("Invalid order format. Use: order <symbol> <side> <quantity> <price>");
                }
            } else if (command.substr(0, 6) == "cancel") {
                std::stringstream ss(command.substr(7));
                uint64_t order_id;
                
                if (ss >> order_id) {
                    send_cancel_order(order_id);
                } else {
                    log_error("Invalid cancel format. Use: cancel <order_id>");
                }
            } else if (command.substr(0, 6) == "market") {
                std::stringstream ss(command.substr(7));
                std::string symbol;
                uint64_t bid, ask;
                
                if (ss >> symbol >> bid >> ask) {
                    send_market_data(symbol, bid, 1000, ask, 1000);
                } else {
                    log_error("Invalid market data format. Use: market <symbol> <bid> <ask>");
                }
            } else if (command == "heartbeat") {
                send_heartbeat();
            } else if (!command.empty()) {
                log_error("Unknown command: " + command);
            }
        }
    }
    
private:
    std::string server_ip_;
    uint16_t server_port_;
    int socket_fd_;
    std::atomic<uint64_t> message_counter_;
    
    // Logging functions with timestamps
    void log_info(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::cout << "\033[34m[INFO]\033[0m [" 
                  << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                  << "." << std::setfill('0') << std::setw(3) << ms.count() 
                  << "] " << message << std::endl;
    }
    
    void log_success(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::cout << "\033[32m[SUCCESS]\033[0m [" 
                  << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                  << "." << std::setfill('0') << std::setw(3) << ms.count() 
                  << "] " << message << std::endl;
    }
    
    void log_error(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::cout << "\033[31m[ERROR]\033[0m [" 
                  << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                  << "." << std::setfill('0') << std::setw(3) << ms.count() 
                  << "] " << message << std::endl;
    }
    
    uint64_t generate_message_id() {
        return ++message_counter_;
    }
    
    uint64_t generate_order_id() {
        return 1000000 + (++message_counter_);
    }
};

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    std::string test_mode = "comprehensive";
    int num_orders = 1000;
    int num_market_updates = 100;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            server_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--mode" && i + 1 < argc) {
            test_mode = argv[++i];
        } else if (arg == "--orders" && i + 1 < argc) {
            num_orders = std::stoi(argv[++i]);
        } else if (arg == "--market" && i + 1 < argc) {
            num_market_updates = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --ip <ip>        Server IP address (default: 127.0.0.1)\n"
                      << "  --port <port>    Server port (default: 8888)\n"
                      << "  --mode <mode>    Test mode: comprehensive, performance, market, interactive (default: comprehensive)\n"
                      << "  --orders <n>     Number of orders for performance test (default: 1000)\n"
                      << "  --market <n>     Number of market updates (default: 100)\n"
                      << "  --help           Show this help message\n";
            return 0;
        }
    }
    
    std::cout << "=== HFT Test Client ===" << std::endl;
    std::cout << "Server: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Mode: " << test_mode << std::endl;
    std::cout << "======================" << std::endl;
    
    TestClient client(server_ip, server_port);
    
    if (test_mode == "comprehensive") {
        client.run_comprehensive_test();
    } else if (test_mode == "performance") {
        if (client.connect()) {
            client.run_performance_test(num_orders);
        }
    } else if (test_mode == "market") {
        if (client.connect()) {
            client.run_market_data_test(num_market_updates);
        }
    } else if (test_mode == "interactive") {
        if (client.connect()) {
            client.interactive_mode();
        }
    } else {
        std::cerr << "Unknown test mode: " << test_mode << std::endl;
        return 1;
    }
    
    std::cout << "Test completed successfully" << std::endl;
    return 0;
} 