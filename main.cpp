#include "hft_server.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <chrono>
#include <thread>

namespace hft {

// Global server instance for signal handling
HFTServer* g_server = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (g_server && signal == SIGINT) {
        std::cout << "\nReceived SIGINT, shutting down gracefully..." << std::endl;
        g_server->stop();
    }
}

} // namespace hft

int main(int argc, char* argv[]) {
    using namespace hft;
    
    // Server configuration
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    size_t thread_count = 4;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            server_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--threads" && i + 1 < argc) {
            thread_count = std::stoul(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --ip <ip>        Server IP address (default: 127.0.0.1)\n"
                      << "  --port <port>    Server port (default: 8888)\n"
                      << "  --threads <n>    Number of worker threads (default: 4)\n"
                      << "  --help           Show this help message\n";
            return 0;
        }
    }
    
    std::cout << "=== HFT Server Starting ===" << std::endl;
    std::cout << "Server IP: " << server_ip << std::endl;
    std::cout << "Server Port: " << server_port << std::endl;
    std::cout << "Worker Threads: " << thread_count << std::endl;
    std::cout << "Target Latency: < 20μs" << std::endl;
    std::cout << "==========================" << std::endl;
    
    // Get server instance
    HFTServer& server = HFTServer::get_instance();
    g_server = &server;
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize server
    if (!server.initialize(server_ip, server_port, thread_count)) {
        std::cerr << "Failed to initialize HFT server" << std::endl;
        return 1;
    }
    
    // Create and register services
    auto order_service = std::make_shared<OrderService>();
    auto market_data_service = std::make_shared<MarketDataService>();
    
    server.register_service(MessageType::ORDER_NEW, order_service);
    server.register_service(MessageType::ORDER_CANCEL, order_service);
    server.register_service(MessageType::ORDER_REPLACE, order_service);
    server.register_service(MessageType::MARKET_DATA, market_data_service);
    
    std::cout << "Services registered successfully" << std::endl;
    
    // Start server
    server.start();
    
    // Main server loop with statistics reporting
    auto last_stats_time = std::chrono::steady_clock::now();
    const auto stats_interval = std::chrono::seconds(5);
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time >= stats_interval) {
            auto stats = server.get_stats();
            
            std::cout << "\n=== Server Statistics ===" << std::endl;
            std::cout << "Total Messages: " << stats.total_messages_processed << std::endl;
            std::cout << "Active Connections: " << stats.total_connections << std::endl;
            std::cout << "Peak Connections: " << stats.peak_connections << std::endl;
            std::cout << "Average Latency: " << std::fixed << std::setprecision(2) 
                      << stats.avg_latency_us << " μs" << std::endl;
            
            // Check if we're meeting latency requirements
            if (stats.avg_latency_us < 20.0) {
                std::cout << "✓ Latency target met (< 20μs)" << std::endl;
            } else {
                std::cout << "⚠ Latency target exceeded: " << stats.avg_latency_us << "μs" << std::endl;
            }
            std::cout << "========================" << std::endl;
            
            last_stats_time = now;
        }
        
        // Check if server is still running
        if (!g_server) {
            break;
        }
    }
    
    std::cout << "HFT Server shutdown complete" << std::endl;
    return 0;
} 