#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include "flatPool.h"
#include "floatOp.h"
#include "message.h"
#include "tscClock.h"
#include "type.h"
#include "zAllocator.h"

// not thread safe
struct Broker {
    using BidsT = std::map<Price, Qty, std::greater<Price>, zAllocator<std::pair<const Price, Qty>>>;
    using AsksT = std::map<Price, Qty, std::less<Price>, zAllocator<std::pair<const Price, Qty>>>;

    Broker() = default;
    Broker(Broker &&) = delete;
    Broker(const Broker &) = delete;
    Broker &operator=(Broker &&) = delete;
    Broker &operator=(const Broker &) = delete;

    void insertOrder(const Order &order) {
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
    void getOrderBook(size_t depth, Orderbook<DEPTH> &obRef) const {
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
    void onLimitBuyOrder(const Order &buyOrder) {
        Qty remainQty = buyOrder.remainQty_;
        const bool shouldBeMatch = !lessThan(buyOrder.price_, bestAskPrice_);
        if (shouldBeMatch) {
            bool shoulUpdateBestAsk = false;
            Price bestAskPrice = bestAskPrice_;
            for (auto it = asks_.begin(); it != asks_.upper_bound(buyOrder.price_);) {
                if (it->second > remainQty) [[likely]] {
                    bestAskPrice = it->first;
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    shoulUpdateBestAsk = true;
                    remainQty -= it->second;
                    it = asks_.erase(it);
                }
            }

            if (shoulUpdateBestAsk) {
                updateAskPriceBoundary(bestAskPrice);
            }
        }

        // consider that time priority including GTC/FOK/IOC,
        // only GTC&FAK order be handled here
        if (remainQty) {
            updateBidOrderBook(buyOrder, remainQty);
        }
    }

    void onCancelLimitBuyOrder(const Order &buyOrder) {
        // at first, should determine whether the entry exist in order book
        // order should be stored in hashmap
        Qty remainQty = buyOrder.remainQty_;
        auto it = bids_.find(buyOrder.price_);
        if (it != bids_.end()) {
            if (it->second > remainQty) {
                it->second -= remainQty;
            } else {
                it = bids_.erase(it);
                updateBidPriceBoundary(it, buyOrder.price_);
            }
        }
        // if shouldBeCancel == false then send error rsp to trader in matching engine
    }

    void onLimitSellOrder(const Order &sellOrder) {
        Qty remainQty = sellOrder.remainQty_;
        const bool shouldBeMatch = !greator(sellOrder.price_, bestBidPrice_);
        if (shouldBeMatch) {
            bool shoulUpdateBestBid = false;
            Price bestBidPrice = bestBidPrice_;
            for (auto it = bids_.begin(); it != bids_.upper_bound(sellOrder.price_);) {
                if (it->second > remainQty) [[likely]] {
                    bestBidPrice = it->first;
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    shoulUpdateBestBid = true;
                    remainQty -= it->second;
                    it = bids_.erase(it);
                }
            }

            if (shoulUpdateBestBid)  {
                updateBidPriceBoundary(bestBidPrice);
            }
        }

        // consider that time priority including GTC/FOK/IOC,
        // only GTC&FAK order be handled here
        if (remainQty) {
            updateAskOrderBook(sellOrder, remainQty);
        }
    }

    void onCancelLimitSellOrder(const Order &sellOrder) {
        // at first, should determine whether the entry exist in order book
        Qty remainQty = sellOrder.remainQty_;
        auto it = asks_.find(sellOrder.price_);
        if (it != asks_.end()) {
            if (it->second > remainQty) {
                it->second -= remainQty;
            } else {
                it = asks_.erase(it);
                updateAskPriceBoundary(it, sellOrder.price_);
            }
        }
    }

    // Futures contracts for market orders to be limited to 1% worse than the best bid or ask
    // protect traders from things like slippage and “fat finger trade” (trader mistakes).
    void onMarketBuyOrder(const Order &buyOrder) {
        Qty remainQty = buyOrder.remainQty_;
        const bool shouldBeMatch = lessThan(buyOrder.price_, bestAskPrice_);
        if (shouldBeMatch) {
            bool shoulUpdateBestAsk = false;
            Price bestAskPrice = bestAskPrice_;
            for (auto it = asks_.begin(); it != asks_.end();) {
                if (it->second > remainQty) {
                    bestAskPrice = it->first;
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    // when filled qty hit 1% of total limit order qty should give up fill
                    shoulUpdateBestAsk = true;
                    remainQty -= it->second;
                    it = asks_.erase(it);
                }
            }

            if (shoulUpdateBestAsk) [[unlikely]] {
                updateAskPriceBoundary(bestAskPrice);
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
        if (shouldBeMatch) {
            bool shoulUpdateBestBid = false;
            Price bestBidPrice = bestBidPrice_;
            for (auto it = bids_.begin(); it != bids_.end();) {
                if (it->second > remainQty) {
                    bestBidPrice = it->first;
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    // when filled qty hit 1% of total limit order qty should give up fill
                    shoulUpdateBestBid = true;
                    remainQty -= it->second;
                    it = bids_.erase(it);
                }
            }

            if (shoulUpdateBestBid) [[unlikely]] {
                updateBidPriceBoundary(bestBidPrice);
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

    void updateBidPriceBoundary(BidsT::iterator &it, Price price) {
        if (!bids_.empty()) {
            const bool equalToUpper = equal(price, bestBidPrice_);
            if (equalToUpper) {
                bestBidPrice_ = it->first;
            }
        } else {
            bestBidPrice_ = std::numeric_limits<Price>::min();
        }
    }

    void updateAskPriceBoundary(AsksT::iterator &it, Price price) {
        if (!asks_.empty()) {
            const bool equalToLower = equal(price, bestAskPrice_);
            if (equalToLower) {
                bestAskPrice_ = it->first;
            }
        } else {
            bestAskPrice_ = std::numeric_limits<Price>::max();
        }
    }

    void updateBidPriceBoundary(Price price) {
        if (!bids_.empty()) [[likely]] {
            bestBidPrice_ = price;
        } else {
            bestBidPrice_ = std::numeric_limits<Price>::min();
        }
    }

    void updateAskPriceBoundary(Price price) {
        if (!asks_.empty()) [[likely]] {
            bestAskPrice_ = price;
        } else {
            bestAskPrice_ = std::numeric_limits<Price>::max();
        }
    }

    void updateAskOrderBook(const Order &orderRef, Qty remainQty) {
        auto result = asks_.emplace(orderRef.price_, remainQty);
        if (!result.second) {
            result.first->second += remainQty;
        } else {
            if (lessThan(orderRef.price_, bestAskPrice_)) {
                bestAskPrice_ = orderRef.price_;
            }
        }
    }

    void updateBidOrderBook(const Order &orderRef, Qty remainQty) {
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
    /*alignas(kDefaultCacheLineSize) Price bestBidPrice_ = std::numeric_limits<Price>::min();
    alignas(kDefaultCacheLineSize) BidsT bids_;

    alignas(kDefaultCacheLineSize) Price bestAskPrice_ = std::numeric_limits<Price>::max();
    alignas(kDefaultCacheLineSize) AsksT asks_;*/

    alignas(kDefaultCacheLineSize) Price bestBidPrice_ = std::numeric_limits<Price>::min();
    //Price bestBidPrice_ = std::numeric_limits<Price>::min();
    alignas(kDefaultCacheLineSize) BidsT bids_;

    alignas(kDefaultCacheLineSize) Price bestAskPrice_ = std::numeric_limits<Price>::max();
    //Price bestAskPrice_ = std::numeric_limits<Price>::max();
    alignas(kDefaultCacheLineSize) AsksT asks_;
};

