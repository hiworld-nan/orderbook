#pragma once

#include <cstdint>
#include <iostream>
#include <ostream>
#include "orderBookInline.h"

using namespace std;
inline std::ostream &operator<<(std::ostream &out, const ClientOrderID &coid) {
    out << coid.value_;
    return out;
}

inline std::ostream &operator<<(std::ostream &out, const Order &order) {
    out << " coid :" << order.coid_ << " ts:" << order.createTimeNs_ << " price:" << order.price_
        << " qty:" << order.qty_ << " remainQty:" << order.remainQty_ << " offset:" << toString(order.offset_)
        << " side:" << toString(order.side_) << " type:" << toString(order.type_)
        << " timeInForce:" << toString(order.tif_);
    return out;
}

template <size_t DEPTH>
void showOrderBook(const Orderbook<DEPTH> &ob) {
    std::cout << "===============orderbook::asks===============" << std::endl;
    for (auto i = 0; i < ob.askSize_; i++) {
        const PriceLevel &priceLevelRef = ob.ask(i);
        std::cout << "price:" << priceLevelRef.price_ << " qty:" << priceLevelRef.qty_ << std::endl;
    }
    std::cout << "===============orderbook::bids===============" << std::endl;
    for (auto i = 0; i < ob.bidSize_; i++) {
        const PriceLevel &priceLevelRef = ob.bid(i);
        std::cout << "price:" << priceLevelRef.price_ << " qty:" << priceLevelRef.qty_ << std::endl;
    }
    std::cout << "=============================================" << std::endl;
}
