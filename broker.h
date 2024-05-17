#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include "floatOp.h"
#include "message.h"
#include "tscClock.h"
#include "zAllocator.h"

// not thread safe
struct Broker {
    // todo: instead of std::map with absl::btree_map
    using BidsT = std::map<Price, Qty, std::greater<Price>, zAllocator<std::pair<const Price, Qty>>>;
    using AsksT = std::map<Price, Qty, std::less<Price>, zAllocator<std::pair<const Price, Qty>>>;

    Broker() = default;
    Broker(Broker &&) = delete;
    Broker(const Broker &) = delete;
    Broker &operator=(Broker &&) = delete;
    Broker &operator=(const Broker &) = delete;

    HintHot void insertOrder(const Order &order) {
        /*  lookup table avoid switch case, for performance but useless for readability
            and actually it's invalid for performance improvement, need to verify again

            using MemFuncT = void (Broker::*)(const Order &);
            static constexpr int32_t kFunNum = 4;
            static constexpr MemFuncT FuncTab[kFunNum] = {&Broker::onLimitBuyOrder, &Broker::onLimitSellOrder,
                                                        &Broker::onMarketBuyOrder, &Broker::onMarketSellOrder};
            const int32_t funcIndex = ((order.type_ > OrderType::Limit) << 1) | (order.side_ > QuoteType::Buy);
            MemFuncT func = FuncTab[funcIndex];
            return (this->*func)(order);
        */

        switch (order.type_) {
            case OrderType::Limit: {
                switch (order.side_) {
                    case QuoteType::Buy:
                        return onLimitBuyOrder(order);

                    case QuoteType::Sell:
                        return onLimitSellOrder(order);

                    default:
                        break;
                }

                case OrderType::Market: {
                    switch (order.side_) {
                        case QuoteType::Buy:
                            return onMarketBuyOrder(order);

                        case QuoteType::Sell:
                            return onMarketSellOrder(order);

                        default:
                            break;
                    }
                }

                default:
                    break;
            }
        }
    }

    void cancelOrder(const Order &order) {
        if (order.orderStatus_ != OrderStatus::Canceled) {
            return;
        }

        switch (order.type_) {
            case OrderType::Limit: {
                switch (order.side_) {
                    case QuoteType::Buy:
                        return onCancelLimitBuyOrder(order);

                    case QuoteType::Sell:
                        return onCancelLimitSellOrder(order);

                    default:
                        break;
                }
                // ignore market order
            }

            default:
                break;
        }
    }

    template <size_t DEPTH>
    void getOrderBook(Orderbook<DEPTH> &obRef, size_t depth = DEPTH) const {
        size_t i = 0;
        const size_t constMaxDepth = (depth > DEPTH) ? DEPTH : depth;
        for (auto it = bids_.begin(); it != bids_.end() && i < constMaxDepth; it++) {
            PriceLevel &priceLevelRef = obRef.bid(i++);
            priceLevelRef.price_ = it->first;
            priceLevelRef.qty_ = it->second;
        }
        obRef.bidSize_ = i;

        i = 0;
        for (auto it = asks_.begin(); it != asks_.end() && i < constMaxDepth; it++) {
            PriceLevel &priceLevelRef = obRef.ask(i++);
            priceLevelRef.price_ = it->first;
            priceLevelRef.qty_ = it->second;
        }
        obRef.askSize_ = i;
    }

   private:
    HintHot void onLimitBuyOrder(const Order &buyOrder) {
        Qty remainQty = buyOrder.remainQty_;
        const bool shouldBeMatch = !lessThan(buyOrder.price_, bestAskPrice_);
        if (shouldBeMatch) [[likely]] {
            for (auto it = asks_.begin(); it != asks_.upper_bound(buyOrder.price_);) {
                if (it->second > remainQty) [[likely]] {
                    bestAskPrice_ = it->first;
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    remainQty -= it->second;
                    it = asks_.erase(it);
                }
            }

            if (asks_.empty()) [[unlikely]] {
                bestAskPrice_ = std::numeric_limits<Price>::max();
            }
        }

        // consider that time priority including GTC/FOK/IOC,
        // only GTC&FAK order be handled here
        if (remainQty) {
            updateBids(buyOrder, remainQty);
        }
    }

    void onCancelLimitBuyOrder(const Order &buyOrder) {
        // at first, should determine whether the entry exist in order book
        // order should be stored in hashmap
        Qty remainQty = buyOrder.remainQty_;
        auto it = bids_.find(buyOrder.price_);
        if (it != bids_.end()) {
            if (it->second > remainQty) [[likely]] {
                it->second -= remainQty;
            } else {
                it = bids_.erase(it);
                updateBestBidPrice(it, buyOrder.price_);
            }
        }
    }

    HintHot void onLimitSellOrder(const Order &sellOrder) {
        Qty remainQty = sellOrder.remainQty_;
        const bool shouldBeMatch = !greator(sellOrder.price_, bestBidPrice_);
        if (shouldBeMatch) [[likely]] {
            for (auto it = bids_.begin(); it != bids_.upper_bound(sellOrder.price_);) {
                if (it->second > remainQty) [[likely]] {
                    bestBidPrice_ = it->first;
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    remainQty -= it->second;
                    it = bids_.erase(it);
                }
            }

            if (bids_.empty()) [[unlikely]] {
                bestBidPrice_ = std::numeric_limits<Price>::min();
            }
        }

        // consider that time priority including GTC/FOK/IOC,
        // only GTC&FAK order be handled here
        if (remainQty) {
            updateAsks(sellOrder, remainQty);
        }
    }

    void onCancelLimitSellOrder(const Order &sellOrder) {
        // at first, should determine whether the entry exist in order book
        Qty remainQty = sellOrder.remainQty_;
        auto it = asks_.find(sellOrder.price_);
        if (it != asks_.end()) {
            if (it->second > remainQty) [[likely]] {
                it->second -= remainQty;
            } else {
                it = asks_.erase(it);
                updateBestAskPrice(it, sellOrder.price_);
            }
        }
    }

    // Futures contracts for market orders to be limited to 1% worse than the best bid or ask
    // protect traders from things like slippage and “fat finger trade” (trader mistakes).
    void onMarketBuyOrder(const Order &buyOrder) {
        Qty remainQty = buyOrder.remainQty_;
        const bool shouldBeMatch = lessThan(buyOrder.price_, bestAskPrice_);
        if (shouldBeMatch) [[likely]] {
            for (auto it = asks_.begin(); it != asks_.end();) {
                if (it->second > remainQty) [[likely]] {
                    bestAskPrice_ = it->first;
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    // when filled qty hit 1% of total limit order qty should give up fill
                    remainQty -= it->second;
                    it = asks_.erase(it);
                }
            }

            if (asks_.empty()) [[unlikely]] {
                bestAskPrice_ = std::numeric_limits<Price>::max();
            }
        }

        // transfer market order into limit order
        // when filled qty hit 1% of total limit order qty
        // time priority of market order should be IOC
        // so remainQty must be cancelled or
        // transfer market order into limit order
        // do not send orderRsp and trade to trader
        // remainQty of marketOrder great than 0 is impossible
        // risk control should handle this
    }

    // Futures contracts for market orders to be limited
    // to 1% worse than the best bid or ask
    // protect traders from things like slippage
    // and “fat finger trade” (trader mistakes).
    void onMarketSellOrder(const Order &sellOrder) {
        Qty remainQty = sellOrder.remainQty_;
        const bool shouldBeMatch = !greator(sellOrder.price_, bestBidPrice_);
        if (shouldBeMatch) [[likely]] {
            for (auto it = bids_.begin(); it != bids_.end();) {
                if (it->second > remainQty) [[likely]] {
                    bestBidPrice_ = it->first;
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    // when filled qty hit 1% of total limit order qty should give up fill
                    remainQty -= it->second;
                    it = bids_.erase(it);
                }
            }

            if (bids_.empty()) [[unlikely]] {
                bestBidPrice_ = std::numeric_limits<Price>::min();
            }
        }

        // transfer market order into limit order
        // when filled qty hit 1% of total limit order qty
        // time priority of market order should be IOC
        // so remainQty must be cancelled
        // or transfer market order into limit order
        // do not send orderRsp and trade to trader
        // remainQty of marketOrder great than 0 is impossible
        // risk control should handle this
    }

    inline void updateBestBidPrice(BidsT::iterator &it, Price price) {
        if (!bids_.empty()) [[likely]] {
            if (equal(price, bestBidPrice_)) [[unlikely]] {
                bestBidPrice_ = it->first;
            }
        } else {
            bestBidPrice_ = std::numeric_limits<Price>::min();
        }
    }

    inline void updateBestAskPrice(AsksT::iterator &it, Price price) {
        if (!asks_.empty()) [[likely]] {
            if (equal(price, bestAskPrice_)) [[unlikely]] {
                bestAskPrice_ = it->first;
            }
        } else {
            bestAskPrice_ = std::numeric_limits<Price>::max();
        }
    }

    void updateAsks(const Order &orderRef, Qty remainQty) {
        auto result = asks_.emplace(orderRef.price_, remainQty);
        if (!result.second) {
            result.first->second += remainQty;
        } else {
            if (lessThan(orderRef.price_, bestAskPrice_)) {
                bestAskPrice_ = orderRef.price_;
            }
        }
    }

    void updateBids(const Order &orderRef, Qty remainQty) {
        auto result = bids_.emplace(orderRef.price_, remainQty);
        if (!result.second) {
            result.first->second += remainQty;
        } else {
            if (greator(orderRef.price_, bestBidPrice_)) {
                bestBidPrice_ = orderRef.price_;
            }
        }
    }

   private:
    Price bestBidPrice_ = std::numeric_limits<Price>::min();
    BidsT bids_;

    Price bestAskPrice_ = std::numeric_limits<Price>::max();
    AsksT asks_;
};
