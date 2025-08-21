#include "ultra_hft_server.h"
#include <iostream>
#include <csignal>
#include <cstring>
#include <cstdlib>

namespace ultra_hft {

// Global server instance for signal handling
UltraHFTServer* g_server = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (g_server) {
        std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
        g_server->stop();
    }
}

// Print usage information
void print_usage(const char* program_name) {
    std::cout << "Ultra HFT Server - Ultra-Low Latency High-Frequency Trading Server" << std::endl;
    std::cout << "=================================================================" << std::endl;
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --ip <ip>        Server IP address (default: 127.0.0.1)" << std::endl;
    std::cout << "  --port <port>    Server port (default: 8888)" << std::endl;
    std::cout << "  --threads <n>    Number of worker threads (default: 4)" << std::endl;
    std::cout << "  --help           Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  • Lock-free queues for maximum performance" << std::endl;
    std::cout << "  • Cache-line aligned data structures" << std::endl;
    std::cout << "  • Zero-copy message processing" << std::endl;
    std::cout << "  • Sub-10μs latency target" << std::endl;
    std::cout << "  • Pre-allocated buffers" << std::endl;
    std::cout << "  • Atomic operations throughout" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " --ip 0.0.0.0 --port 9999 --threads 8" << std::endl;
}

// Parse command line arguments
bool parse_arguments(int argc, char* argv[], std::string& ip, uint16_t& port, uint32_t& threads) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (strcmp(argv[i], "--ip") == 0) {
            if (i + 1 < argc) {
                ip = argv[++i];
            } else {
                std::cerr << "Error: --ip requires an argument" << std::endl;
                return false;
            }
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                port = static_cast<uint16_t>(atoi(argv[++i]));
                if (port == 0) {
                    std::cerr << "Error: Invalid port number" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "Error: --port requires an argument" << std::endl;
                return false;
            }
        } else if (strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) {
                threads = static_cast<uint32_t>(atoi(argv[++i]));
                if (threads == 0) {
                    std::cerr << "Error: Invalid thread count" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "Error: --threads requires an argument" << std::endl;
                return false;
            }
        } else {
            std::cerr << "Error: Unknown option " << argv[i] << std::endl;
            print_usage(argv[0]);
            return false;
        }
    }
    return true;
}

} // namespace ultra_hft

int main(int argc, char* argv[]) {
    using namespace ultra_hft;
    
    // Default configuration
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    uint32_t thread_count = 4;
    
    // Parse command line arguments
    if (!parse_arguments(argc, argv, server_ip, server_port, thread_count)) {
        return 1;
    }
    
    // Create server instance
    UltraHFTServer server(server_ip, server_port, thread_count);
    g_server = &server;
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "Starting Ultra HFT Server..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // Initialize server
    if (!server.initialize()) {
        std::cerr << "Failed to initialize Ultra HFT Server" << std::endl;
        return 1;
    }
    
    // Start server
    if (!server.start()) {
        std::cerr << "Failed to start Ultra HFT Server" << std::endl;
        return 1;
    }
    
    // Main loop - wait for shutdown signal
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check if server is still running
        if (!g_server) {
            break;
        }
    }
    
    std::cout << "Ultra HFT Server shutdown complete" << std::endl;
    return 0;
} 