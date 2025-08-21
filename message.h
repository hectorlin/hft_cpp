#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <string>
#include <chrono>
#include <array>

namespace hft {

/**
 * @brief Message types for different trading operations
 */
enum class MessageType : uint8_t {
    ORDER_NEW = 0x01,
    ORDER_CANCEL = 0x02,
    ORDER_REPLACE = 0x03,
    ORDER_FILL = 0x04,
    ORDER_REJECT = 0x05,
    MARKET_DATA = 0x06,
    HEARTBEAT = 0x07,
    LOGIN = 0x08,
    LOGOUT = 0x09,
    ERROR = 0xFF
};

/**
 * @brief Order side (buy/sell)
 */
enum class OrderSide : uint8_t {
    BUY = 0x01,
    SELL = 0x02
};

/**
 * @brief Order type
 */
enum class OrderType : uint8_t {
    MARKET = 0x01,
    LIMIT = 0x02,
    STOP = 0x03,
    STOP_LIMIT = 0x04
};

/**
 * @brief Time in force
 */
enum class TimeInForce : uint8_t {
    DAY = 0x01,
    IOC = 0x02,  // Immediate or Cancel
    FOK = 0x03,  // Fill or Kill
    GTC = 0x04   // Good Till Cancel
};

/**
 * @brief Message status
 */
enum class MessageStatus : uint8_t {
    PENDING = 0x01,
    PROCESSED = 0x02,
    COMPLETED = 0x03,
    FAILED = 0x04
};

/**
 * @brief Base message structure for all HFT communications
 */
struct Message {
    // Header fields
    uint64_t message_id;           // Unique message identifier
    uint64_t timestamp;            // Timestamp in nanoseconds since epoch
    uint32_t sequence_number;      // Sequence number for ordering
    MessageType message_type;      // Type of message
    MessageStatus status;          // Current status of the message
    uint32_t source_id;            // Source system identifier
    uint32_t destination_id;       // Destination system identifier
    
    // Payload size and data
    uint32_t payload_size;         // Size of payload in bytes
    std::array<uint8_t, 1024> payload; // Fixed-size payload buffer
    
    // Constructor
    Message() : message_id(0), timestamp(0), sequence_number(0), 
                message_type(MessageType::HEARTBEAT), status(MessageStatus::PENDING),
                source_id(0), destination_id(0), payload_size(0) {
        payload.fill(0);
    }
    
    // Copy constructor
    Message(const Message& other) = default;
    
    // Assignment operator
    Message& operator=(const Message& other) = default;
    
    // Destructor
    ~Message() = default;
    
    /**
     * @brief Get current timestamp in nanoseconds
     */
    static uint64_t get_current_timestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }
    
    /**
     * @brief Update timestamp to current time
     */
    void update_timestamp() {
        timestamp = get_current_timestamp();
    }
    
    /**
     * @brief Check if message is valid
     */
    bool is_valid() const {
        return message_id != 0 && timestamp != 0 && payload_size <= payload.size();
    }
    
    /**
     * @brief Clear message data
     */
    void clear() {
        message_id = 0;
        timestamp = 0;
        sequence_number = 0;
        message_type = MessageType::HEARTBEAT;
        status = MessageStatus::PENDING;
        source_id = 0;
        destination_id = 0;
        payload_size = 0;
        payload.fill(0);
    }
};

/**
 * @brief Order message structure
 */
struct OrderMessage : public Message {
    // Order specific fields
    std::array<char, 16> symbol;      // Trading symbol
    OrderSide side;                    // Buy or Sell
    OrderType order_type;              // Market, Limit, etc.
    TimeInForce time_in_force;         // Time in force
    uint64_t order_id;                 // Unique order identifier
    uint64_t client_order_id;          // Client's order identifier
    uint32_t quantity;                 // Order quantity
    uint64_t price;                    // Order price (in ticks)
    uint64_t stop_price;               // Stop price for stop orders
    
    OrderMessage() : side(OrderSide::BUY), order_type(OrderType::LIMIT),
                    time_in_force(TimeInForce::DAY), order_id(0), client_order_id(0),
                    quantity(0), price(0), stop_price(0) {
        message_type = MessageType::ORDER_NEW;
        symbol.fill('\0');
    }
};

/**
 * @brief Market data message structure
 */
struct MarketDataMessage : public Message {
    // Market data fields
    std::array<char, 16> symbol;      // Trading symbol
    uint64_t bid_price;               // Best bid price
    uint32_t bid_size;                // Best bid size
    uint64_t ask_price;               // Best ask price
    uint32_t ask_size;                // Best ask size
    uint64_t last_price;              // Last traded price
    uint32_t last_size;               // Last traded size
    uint64_t volume;                  // Total volume
    uint64_t high_price;              // High price
    uint64_t low_price;                // Low price
    
    MarketDataMessage() : bid_price(0), bid_size(0), ask_price(0), ask_size(0),
                          last_price(0), last_size(0), volume(0), high_price(0), low_price(0) {
        message_type = MessageType::MARKET_DATA;
        symbol.fill('\0');
    }
};

/**
 * @brief Fill message structure
 */
struct FillMessage : public Message {
    // Fill specific fields
    uint64_t order_id;                // Original order ID
    uint64_t fill_id;                 // Unique fill identifier
    uint32_t fill_quantity;           // Quantity filled
    uint64_t fill_price;              // Price at which filled
    uint64_t commission;              // Commission amount
    std::array<char, 16> execution_venue; // Execution venue
    
    FillMessage() : order_id(0), fill_id(0), fill_quantity(0), 
                    fill_price(0), commission(0) {
        message_type = MessageType::ORDER_FILL;
        execution_venue.fill('\0');
    }
};

} // namespace hft

#endif // MESSAGE_H 