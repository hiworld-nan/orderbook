#pragma once

#include <cstddef>
#include <cstdint>
#include "floatOp.h"
#include "type.h"

static constexpr int16_t skDefaultIDLen = 32;

union uCombinedAcctID {
    uint32_t id_ = 0;
    struct {
        uint16_t traderID_ : 16;
        uint16_t logicAcctID_ : 16;
    } breakdown;

    uCombinedAcctID(uint32_t v) : id_(v) {}
    uCombinedAcctID &operator=(uint32_t v) {
        id_ = v;
        return *this;
    }
} __attribute__((packed));

union ClientOrderID {
    uint64_t value_ = 0;
    struct {
        uint32_t combAcctID_;
        // second range from 0 to 86,400 = 24*60*60
        uint64_t timeSec_ : 18;
        // counter range from 0 to 32768 = 2^15
        uint64_t seqNum_ : 14;
    } breakdown;
    ClientOrderID(uint64_t v) : value_(v) {}
    ClientOrderID &operator=(uint64_t v) {
        value_ = v;
        return *this;
    }
} __attribute__((packed));

struct Trade {
    uint64_t tradeId_ = 0;
    Price price_ = 0;
    Qty qty_ = 0;
    int32_t bidOrderId_ = 0;
    int32_t askOrderId_ = 0;
} __attribute__((packed));

struct InsertOrder {
    ClientOrderID coid_ = 0;
    Nanoseconds tsNs_ = 0;

    uint32_t sid_ = 0;
    Price price_ = 0;
    Qty qty_ = 0;

    Offset offset_ = Offset::Open;
    QuoteType side_ = QuoteType::Buy;
    OrderType type_ = OrderType::Limit;
    TimeInForce tif_ = TimeInForce::IOC;
} __attribute__((packed));

struct Order {
    uint64_t coid_ = 0;
    // sid_ ==> symbol(instrumentid)
    int32_t sid_ = -1;

    QuoteType side_ = QuoteType::Buy;
    // OrderStatus orderStatus_ = OrderStatus::Unknown;
    OrderType type_ = OrderType::Limit;
    Offset offset_ = Offset::Unknown;
    TimeInForce tif_ = TimeInForce::Unknown;
    // char reserve_[3] = {'\0'};

    float price_ = 0.0;
    int32_t qty_ = 0.0;
    int32_t remainQty_ = 0.0;

    uint64_t createTimeNs_ = 0;
    uint64_t updateTimeNs_ = 0;
    /*
    float latestExecPrice_ = 0.0;
    int32_t latestExecQty_ = 0.0;
    float latestExecfee_ = 0.0;
    int32_t feeSid_ = -1;

    double cumExecFee_ = 0.0;
    int32_t cumExecQty_ = 0.0;
    float avgExecPrice_ = 0.0;*/

    char eoid_[skDefaultIDLen] = {'\0'};
    // char latestFillId_[skDefaultIDLen] = {'\0'};
} __attribute__((packed));

struct PriceLevel {
    Price price_ = 0;
    Qty qty_ = 0;
} __attribute__((packed));

template <size_t N>
struct Orderbook {
    static constexpr size_t skMaxDepth = N;
    using SelfT = Orderbook<N>;
    uint16_t bidSize_ = 0;
    uint16_t askSize_ = 0;
    PriceLevel bids_[N];
    PriceLevel asks_[N];

    PriceLevel &bid(uint32_t i) { return bids_[i]; }
    PriceLevel &ask(uint32_t i) { return asks_[i]; }

    const PriceLevel &bid(uint32_t i) const { return bids_[i]; }
    const PriceLevel &ask(uint32_t i) const { return asks_[i]; }
} __attribute__((packed));

