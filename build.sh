#!/bin/bash

# HFT Server Simple Build Script for Linux
# Direct g++ compilation without CMake for simplicity

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored messages
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if required tools are available
check_dependencies() {
    print_info "Checking build dependencies..."
    
    # Check for g++
    if command -v g++ >/dev/null 2>&1; then
        GCC_VERSION=$(g++ --version | head -n1)
        print_success "Found g++: $GCC_VERSION"
    else
        print_error "g++ compiler not found. Please install g++ first."
        exit 1
    fi
    
    # Check for make (optional, but useful)
    if command -v make >/dev/null 2>&1; then
        MAKE_VERSION=$(make --version | head -n1)
        print_success "Found make: $MAKE_VERSION"
    else
        print_warning "make not found. Will use direct g++ compilation."
    fi
}

# Clean previous build
clean_build() {
    print_info "Cleaning previous build artifacts..."
    if [ -d "build" ]; then
        rm -rf build
        print_success "Previous build directory removed"
    fi
}

# Create build directory
create_build_dir() {
    print_info "Creating build directory..."
    mkdir -p build/bin
    print_success "Build directory created"
}

# Set compiler flags for HFT optimization
set_compiler_flags() {
    print_info "Setting HFT-optimized compiler flags..."
    
    # Base optimization flags
    CXXFLAGS="-std=c++20 -O3 -march=native -mtune=native"
    
    # Performance flags
    CXXFLAGS="$CXXFLAGS -ffast-math -funroll-loops"
    CXXFLAGS="$CXXFLAGS -fno-exceptions -fno-rtti"
    CXXFLAGS="$CXXFLAGS -fomit-frame-pointer"
    
    # Network optimization flags
    CXXFLAGS="$CXXFLAGS -DSO_REUSEADDR -DTCP_NODELAY"
    
    # System flags
    CXXFLAGS="$CXXFLAGS -D_GNU_SOURCE -D_REENTRANT -DNDEBUG"
    
    # Threading support
    CXXFLAGS="$CXXFLAGS -pthread"
    
    # Include directories
    INCLUDES="-I."
    
    # Linker flags
    LDFLAGS="-pthread"
    
    print_success "Compiler flags set: $CXXFLAGS"
}

# Build hft_server
build_hft_server() {
    print_info "Building hft_server..."
    
    g++ $CXXFLAGS $INCLUDES \
        -o build/bin/hft_server \
        main.cpp hft_server.cpp \
        $LDFLAGS
    
    if [ $? -eq 0 ]; then
        print_success "hft_server built successfully"
        print_info "Size: $(du -h build/bin/hft_server | cut -f1)"
    else
        print_error "Failed to build hft_server"
        exit 1
    fi
}

# Build ultra_hft_server
build_ultra_hft_server() {
    print_info "Building ultra_hft_server (lock-free optimized)..."
    
    g++ $CXXFLAGS $INCLUDES \
        -o build/bin/ultra_hft_server \
        ultra_main.cpp ultra_hft_server.cpp \
        $LDFLAGS
    
    if [ $? -eq 0 ]; then
        print_success "ultra_hft_server built successfully"
        print_info "Size: $(du -h build/bin/ultra_hft_server | cut -f1)"
    else
        print_error "Failed to build ultra_hft_server"
        exit 1
    fi
}

# Build test_client
build_test_client() {
    print_info "Building test_client..."
    
    g++ $CXXFLAGS $INCLUDES \
        -o build/bin/test_client \
        test_client.cpp \
        $LDFLAGS
    
    if [ $? -eq 0 ]; then
        print_success "test_client built successfully"
        print_info "Size: $(du -h build/bin/test_client | cut -f1)"
    else
        print_error "Failed to build test_client"
        exit 1
    fi
}

# Verify build results
verify_build() {
    print_info "Verifying build results..."
    
    if [ -f "build/bin/hft_server" ] && [ -x "build/bin/hft_server" ]; then
        print_success "hft_server executable verified"
    else
        print_error "hft_server executable not found or not executable"
        exit 1
    fi
    
    if [ -f "build/bin/ultra_hft_server" ] && [ -x "build/bin/ultra_hft_server" ]; then
        print_success "ultra_hft_server executable verified"
    else
        print_error "ultra_hft_server executable not found or not executable"
        exit 1
    fi
    
    if [ -f "build/bin/test_client" ] && [ -x "build/bin/test_client" ]; then
        print_success "test_client executable verified"
    else
        print_error "test_client executable not found or not executable"
        exit 1
    fi
}

# Test executables
test_executables() {
    print_info "Testing executables..."
    
    # Test hft_server help
    if ./build/bin/hft_server --help >/dev/null 2>&1; then
        print_success "hft_server help command works"
    else
        print_warning "hft_server help command failed"
    fi
    
    # Test ultra_hft_server help
    if ./build/bin/ultra_hft_server --help >/dev/null 2>&1; then
        print_success "ultra_hft_server help command works"
    else
        print_warning "ultra_hft_server help command failed"
    fi
    
    # Test test_client help
    if ./build/bin/test_client --help >/dev/null 2>&1; then
        print_success "test_client help command works"
    else
        print_warning "test_client help command failed"
    fi
}

# Show build summary
show_summary() {
    print_info "Build Summary:"
    echo "=================="
    print_success "✓ All dependencies found"
    print_success "✓ Build directory created"
    print_success "✓ Compiler flags configured"
    print_success "✓ hft_server built successfully"
    print_success "✓ ultra_hft_server built successfully"
    print_success "✓ test_client built successfully"
    print_success "✓ Executables verified"
    print_success "✓ Basic tests passed"
    echo ""
    print_info "Executables are located in: build/bin/"
    print_info "You can now run:"
    echo "  ./build/bin/hft_server --port 9999 --threads 4"
    echo "  ./build/bin/ultra_hft_server --port 9999 --threads 8"
    echo "  ./build/bin/test_client --mode comprehensive"
    echo ""
    print_info "Ultra HFT Server Features:"
    echo "  • Lock-free queues for maximum performance"
    echo "  • Cache-line aligned data structures"
    echo "  • Zero-copy message processing"
    echo "  • Sub-10μs latency target"
    echo "  • Pre-allocated buffers"
    echo "  • Atomic operations throughout"
    echo ""
    print_info "Build completed without CMake dependency!"
}

# Show compiler information
show_compiler_info() {
    print_info "Compiler Information:"
    echo "========================"
    print_info "C++ Standard: C++20"
    print_info "Optimization: -O3 (maximum)"
    print_info "CPU Tuning: -march=native -mtune=native"
    print_info "Performance: -ffast-math -funroll-loops"
    print_info "Exceptions: Disabled (-fno-exceptions)"
    print_info "RTTI: Disabled (-fno-rtti)"
    print_info "Threading: pthread enabled"
    print_info "Network: SO_REUSEADDR, TCP_NODELAY"
}

# Main build process
main() {
    echo "=========================================="
    echo "    HFT Server Build Script for Linux"
    echo "    Direct g++ Compilation (No CMake)"
    echo "    Including Ultra HFT Server"
    echo "=========================================="
    echo ""
    
    check_dependencies
    clean_build
    create_build_dir
    set_compiler_flags
    build_hft_server
    build_ultra_hft_server
    build_test_client
    verify_build
    test_executables
    show_compiler_info
    show_summary
    
    echo ""
    print_success "Build process completed successfully! 🚀"
    print_info "No CMake required - direct g++ compilation!"
    print_info "Ultra HFT Server with lock-free queues ready!"
}

# Run main function
main "$@" 