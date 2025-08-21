cmake_minimum_required(VERSION 3.16)
project(HFTServer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type to Release for production
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific optimizations for ultra-low latency
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Aggressive optimization flags for HFT
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -mtune=native")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math -funroll-loops")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-exceptions -fno-rtti")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fwhole-program")
    
    # Link-time optimization
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
    
    # Additional performance flags
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fomit-frame-pointer")
    
    # Network optimization flags
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DSO_REUSEADDR")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DTCP_NODELAY")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Source files for standard HFT server
set(HFT_SERVER_SOURCES
    main.cpp
    hft_server.cpp
)

# Source files for ultra HFT server
set(ULTRA_HFT_SOURCES
    ultra_main.cpp
    ultra_hft_server.cpp
)

# Test client source
set(TEST_CLIENT_SOURCES
    test_client.cpp
)

# Header files
set(HEADERS
    message.h
    hft_server.h
    ultra_hft_server.h
)

# Create standard HFT server executable
add_executable(hft_server ${HFT_SERVER_SOURCES} ${HEADERS})

# Link libraries for standard HFT server
target_link_libraries(hft_server PRIVATE Threads::Threads)

# Create ultra HFT server executable
add_executable(ultra_hft_server ${ULTRA_HFT_SOURCES} ${HEADERS})

# Link libraries for ultra HFT server
target_link_libraries(ultra_hft_server PRIVATE Threads::Threads)

# Create test client executable
add_executable(test_client ${TEST_CLIENT_SOURCES} ${HEADERS})

# Link libraries for test client
target_link_libraries(test_client PRIVATE Threads::Threads)

# Include directories for all targets
target_include_directories(hft_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ultra_hft_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(test_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Compiler definitions for all targets
target_compile_definitions(hft_server PRIVATE
    _GNU_SOURCE
    _REENTRANT
    NDEBUG
)

target_compile_definitions(ultra_hft_server PRIVATE
    _GNU_SOURCE
    _REENTRANT
    NDEBUG
)

target_compile_definitions(test_client PRIVATE
    _GNU_SOURCE
    _REENTRANT
    NDEBUG
)

# Set output directory for all targets
set_target_properties(hft_server ultra_hft_server test_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install targets
install(TARGETS hft_server ultra_hft_server test_client
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES ${HEADERS}
    DESTINATION include/hft
)

# Print configuration info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Optimization flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

# Add custom target for performance testing
add_custom_target(perf_test
    COMMAND ${CMAKE_BINARY_DIR}/bin/hft_server --help
    COMMAND ${CMAKE_BINARY_DIR}/bin/ultra_hft_server --help
    COMMENT "Testing HFT servers performance configuration"
    DEPENDS hft_server ultra_hft_server
)

# Add custom target for ultra performance testing
add_custom_target(ultra_perf_test
    COMMAND ${CMAKE_BINARY_DIR}/bin/ultra_hft_server --help
    COMMENT "Testing Ultra HFT server lock-free performance configuration"
    DEPENDS ultra_hft_server
)

# Add custom target for clean build
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning all build artifacts"
)

# Print build summary
message(STATUS "Building targets:")
message(STATUS "  - hft_server: Standard HFT server with service architecture")
message(STATUS "  - ultra_hft_server: Ultra HFT server with lock-free queues")
message(STATUS "  - test_client: Comprehensive test client")
message(STATUS "")
message(STATUS "Ultra HFT Server Features:")
message(STATUS "  - Lock-free queues for maximum performance")
message(STATUS "  - Cache-line aligned data structures")
message(STATUS "  - Zero-copy message processing")
message(STATUS "  - Sub-10μs latency target")
message(STATUS "  - Pre-allocated buffers")
message(STATUS "  - Atomic operations throughout") 