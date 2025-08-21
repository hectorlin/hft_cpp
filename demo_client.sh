#!/bin/bash

# HFT Test Client Demonstration Script
# 演示脚本展示测试客户端的不同功能

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== HFT Test Client Demonstration ===${NC}"
echo "This script demonstrates the test client functionality"
echo ""

# Check if binaries exist
if [ ! -f "build/bin/test_client" ]; then
    echo -e "${YELLOW}Test client not found. Building first...${NC}"
    ./build_and_test.sh
fi

echo -e "${GREEN}Starting demonstrations...${NC}"
echo ""

# Demo 1: Show help
echo -e "${BLUE}Demo 1: Help and Usage Information${NC}"
echo "----------------------------------------"
./build/bin/test_client --help
echo ""

# Demo 2: Performance test with small number of orders
echo -e "${BLUE}Demo 2: Performance Test (10 orders)${NC}"
echo "----------------------------------------"
echo "This will test order sending performance with detailed logging"
echo "Press Enter to continue..."
read

./build/bin/test_client --mode performance --orders 10 --port 9999
echo ""

# Demo 3: Market data test
echo -e "${BLUE}Demo 3: Market Data Test (20 updates)${NC}"
echo "----------------------------------------"
echo "This will test market data transmission with detailed logging"
echo "Press Enter to continue..."
read

./build/bin/test_client --mode market --market 20 --port 9999
echo ""

# Demo 4: Interactive mode (brief)
echo -e "${BLUE}Demo 4: Interactive Mode Preview${NC}"
echo "----------------------------------------"
echo "This will show the interactive mode (will exit after 5 seconds)"
echo "Press Enter to continue..."
read

timeout 5s ./build/bin/test_client --mode interactive --port 9999 || true
echo ""

# Demo 5: Comprehensive test
echo -e "${BLUE}Demo 5: Comprehensive Test Suite${NC}"
echo "----------------------------------------"
echo "This will run all tests with detailed logging"
echo "Press Enter to continue..."
read

./build/bin/test_client --mode comprehensive --port 9999
echo ""

echo -e "${GREEN}=== Demonstration Complete ===${NC}"
echo ""
echo -e "${BLUE}Available Test Modes:${NC}"
echo "  --mode comprehensive  - Run all tests (default)"
echo "  --mode performance    - Performance testing"
echo "  --mode market        - Market data testing"
echo "  --mode interactive   - Interactive command mode"
echo ""
echo -e "${BLUE}Usage Examples:${NC}"
echo "  ./build/bin/test_client --mode performance --orders 1000"
echo "  ./build/bin/test_client --mode market --market 500"
echo "  ./build/bin/test_client --mode interactive"
echo "  ./build/bin/test_client --help"
echo ""
echo -e "${GREEN}The test client provides detailed message logging with timestamps${NC}"
echo "All network operations are logged with success/error status"
echo "Performance metrics are calculated and displayed" 