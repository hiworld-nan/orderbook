#pragma once

#include <cstdint>
#include <string_view>

using Price = float;
constexpr Price INVALID_PRICE = 0;
using Qty = int32_t;
constexpr Qty INVALID_QTY = 0;

using Nanoseconds = uint64_t;

enum class QuoteType : int8_t {
    Unknown = 0,
    Buy,
    Sell,
};
constexpr inline std::string_view toString(QuoteType type) {
    switch (type) {
        case QuoteType::Buy:
            return "Buy";
        case QuoteType::Sell:
            return "Sell";
        case QuoteType::Unknown:
            return "Unknown";
    }
    return "";
}

enum class OrderType : int8_t {
    Unknown = 0,
    Limit,
    Market,
};
constexpr inline std::string_view toString(OrderType type) {
    switch (type) {
        case OrderType::Limit:
            return "Limit";
        case OrderType::Market:
            return "Market";
        case OrderType::Unknown:
            return "Unknown";
    }
    return "";
}

enum class TimeInForce : int8_t { Unknown = 0, IOC, GTC, FOK };
constexpr inline std::string_view toString(TimeInForce timeInForce) {
    switch (timeInForce) {
        case TimeInForce::IOC:
            return "IOC";
        case TimeInForce::GTC:
            return "GTC";
        case TimeInForce::FOK:
            return "FOK";
        case TimeInForce::Unknown:
            return "Unknown";
    }
    return "";
}

enum class Offset : int8_t { Unknown, Open, Close };
constexpr inline std::string_view toString(Offset offset) {
    switch (offset) {
        case Offset::Open:
            return "Open";
        case Offset::Close:
            return "Close";
        case Offset::Unknown:
            return "Unknown";
    }
    return "";
}

enum class Direction : int8_t { Unknown, Long, Short, Net };
static inline std::string_view toString(Direction direction) {
    switch (direction) {
        case Direction::Long:
            return "Long";
        case Direction::Short:
            return "Short";
        case Direction::Net:
            return "Net";
        case Direction::Unknown:
            return "Unknown";
    }
    return "Unknown";
}

enum class OrderStatus : int8_t {
    Unknown = 0,
    New = 1,
    PendingNew = 2,
    PendingCancel = 3,
    PartiallyFilled = 4,
    // the following are final order status
    Canceled = 5,
    Rejected = 6,
    Filled = 7,
    InternalRejected = 8,
    Error = 9,
    Closed = 10
};

static inline std::string_view toString(OrderStatus status) {
    switch (status) {
        case OrderStatus::New:
            return "New";
        case OrderStatus::PendingNew:
            return "PendingNew";
        case OrderStatus::PendingCancel:
            return "PendingCancel";
        case OrderStatus::PartiallyFilled:
            return "PartiallyFilled";
        case OrderStatus::Canceled:
            return "Canceled";
        case OrderStatus::Rejected:
            return "Rejected";
        case OrderStatus::Filled:
            return "Filled";
        case OrderStatus::InternalRejected:
            return "InternalRejected";
        case OrderStatus::Error:
            return "Error";
        case OrderStatus::Closed:
            return "Closed";
        case OrderStatus::Unknown:
            return "Unknown";
    }
    return "";
}
