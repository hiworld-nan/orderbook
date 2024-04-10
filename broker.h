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
        const bool shouldBeMatch = !lessThan(buyOrder.price_, askPriceLowerBound_);
        if (shouldBeMatch) {
            for (auto it = asks_.begin(); it != asks_.upper_bound(buyOrder.price_);) {
                if (it->second > remainQty) {
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    remainQty -= it->second;
                    it->second = 0;
                    it = asks_.erase(it);
                    updateAskPriceBoundary(it);
                }
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
        const bool lessThanLower = lessThan(buyOrder.price_, askPriceLowerBound_);
        const bool greaterThanUpper = greator(buyOrder.price_, askPriceLowerBound_);
        const bool shouldBeCancel = !(lessThanLower || greaterThanUpper);
        if (shouldBeCancel) {
            auto it = asks_.find(buyOrder.price_);
            if (it != asks_.end()) {
                if (it->second > remainQty) {
                    it->second -= remainQty;
                } else {
                    it->second = 0;
                    it = asks_.erase(it);
                    updateAskPriceBoundary(it, buyOrder.price_);
                }
            }
        }
        // if shouldBeCancel == false then send error rsp to trader in matching engine
    }

    void onLimitSellOrder(const Order &sellOrder) {
        Qty remainQty = sellOrder.remainQty_;
        const bool shouldBeMatch = !greator(sellOrder.price_, bidPriceUpperBound_);
        if (shouldBeMatch) {
            for (auto it = bids_.begin(); it != bids_.upper_bound(sellOrder.price_);) {
                if (it->second > remainQty) {
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    remainQty -= it->second;
                    it->second = 0;
                    it = bids_.erase(it);
                    updateBidPriceBoundary(it);
                }
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
        const bool lessThanLower = lessThan(sellOrder.price_, bidPriceLowerBound_);
        const bool greaterThanUpper = greator(sellOrder.price_, bidPriceLowerBound_);
        const bool shouldBeCancel = !(lessThanLower || greaterThanUpper);
        if (shouldBeCancel) {
            auto it = bids_.find(sellOrder.price_);
            if (it != bids_.end()) {
                if (it->second > remainQty) {
                    it->second -= remainQty;
                } else {
                    it->second = 0;
                    it = bids_.erase(it);
                    updateBidPriceBoundary(it, sellOrder.price_);
                }
            }
        }
    }

    // Futures contracts for market orders to be limited to 1% worse than the best bid or ask
    // protect traders from things like slippage and “fat finger trade” (trader mistakes).
    void onMarketBuyOrder(const Order &buyOrder) {
        Qty remainQty = buyOrder.remainQty_;
        const bool shouldBeMatch = lessThan(buyOrder.price_, askPriceLowerBound_);
        if (shouldBeMatch) {
            for (auto it = asks_.begin(); it != asks_.end();) {
                if (it->second > remainQty) {
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    // when filled qty hit 1% of total limit order qty should give up fill
                    remainQty -= it->second;
                    it->second = 0;
                    it = asks_.erase(it);
                    updateAskPriceBoundary(it);
                }
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
        const bool shouldBeMatch = !greator(sellOrder.price_, bidPriceUpperBound_);
        if (shouldBeMatch) {
            for (auto it = bids_.begin(); it != bids_.end();) {
                if (it->second > remainQty) {
                    it->second -= remainQty;
                    remainQty = 0;
                    break;
                } else {
                    // when filled qty hit 1% of total limit order qty should give up fill
                    remainQty -= it->second;
                    it->second = 0;
                    it = bids_.erase(it);
                    updateBidPriceBoundary(it);
                }
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
            const bool equalToLower = equal(price, bidPriceLowerBound_);
            const bool equalToUpper = equal(price, bidPriceUpperBound_);
            if (equalToUpper) {
                bidPriceUpperBound_ = it->first;
            }
            if (equalToLower) {
                it--;
                bidPriceLowerBound_ = it->first;
            }
        } else {
            bidPriceLowerBound_ = std::numeric_limits<Price>::max();
            bidPriceUpperBound_ = std::numeric_limits<Price>::min();
        }
    }

    void updateAskPriceBoundary(AsksT::iterator &it, Price price) {
        if (!asks_.empty()) {
            const bool equalToLower = equal(price, askPriceLowerBound_);
            const bool equalToUpper = equal(price, askPriceUpperBound_);
            if (equalToLower) {
                askPriceLowerBound_ = it->first;
            }
            if (equalToUpper) {
                it--;
                askPriceUpperBound_ = it->first;
            }
        } else {
            askPriceLowerBound_ = std::numeric_limits<Price>::max();
            askPriceUpperBound_ = std::numeric_limits<Price>::min();
        }
    }

    void updateBidPriceBoundary(const BidsT::iterator &it) {
        if (!bids_.empty()) {
            bidPriceUpperBound_ = it->first;
        } else {
            bidPriceLowerBound_ = std::numeric_limits<Price>::max();
            bidPriceUpperBound_ = std::numeric_limits<Price>::min();
        }
    }

    void updateAskPriceBoundary(const AsksT::iterator &it) {
        if (!asks_.empty()) {
            askPriceLowerBound_ = it->first;
        } else {
            askPriceLowerBound_ = std::numeric_limits<Price>::max();
            askPriceUpperBound_ = std::numeric_limits<Price>::min();
        }
    }

    void updateAskOrderBook(const Order &orderRef, Qty remainQty) {
        const bool lessThanLower = lessThan(orderRef.price_, askPriceLowerBound_);
        const bool greaterThanUpper = greator(orderRef.price_, askPriceUpperBound_);
        const bool shouldBeEmplace = lessThanLower || greaterThanUpper;
        if (shouldBeEmplace) {
            asks_.emplace(orderRef.price_, remainQty);
            askPriceLowerBound_ = lessThanLower ? orderRef.price_ : askPriceLowerBound_;
            askPriceUpperBound_ = greaterThanUpper ? orderRef.price_ : askPriceUpperBound_;
        } else {
            auto it = asks_.find(orderRef.price_);
            if (it != asks_.end()) {
                it->second += remainQty;
            } else {
                asks_.emplace(orderRef.price_, remainQty);
            }
        }
    }

    void updateBidOrderBook(const Order &orderRef, Qty remainQty) {
        const bool lessThanLower = lessThan(orderRef.price_, bidPriceLowerBound_);
        const bool greaterThanUpper = greator(orderRef.price_, bidPriceUpperBound_);
        const bool shouldBeEmplace = lessThanLower || greaterThanUpper;
        if (shouldBeEmplace) {
            bids_.emplace(orderRef.price_, remainQty);
            bidPriceLowerBound_ = lessThanLower ? orderRef.price_ : bidPriceLowerBound_;
            bidPriceUpperBound_ = greaterThanUpper ? orderRef.price_ : bidPriceUpperBound_;
        } else {
            auto it = bids_.find(orderRef.price_);
            if (it != bids_.end()) {
                it->second += remainQty;
            } else {
                bids_.emplace(orderRef.price_, remainQty);
            }
        }
    }

   private:
    Price bidPriceUpperBound_ = std::numeric_limits<Price>::min();
    Price bidPriceLowerBound_ = std::numeric_limits<Price>::max();
    BidsT bids_;

    Price askPriceUpperBound_ = std::numeric_limits<Price>::min();
    Price askPriceLowerBound_ = std::numeric_limits<Price>::max();
    AsksT asks_;
};
