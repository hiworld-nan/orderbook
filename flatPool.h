#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <utility>
#include "util.h"

using namespace std;

template <class T>
struct FlatPool final {
    using DataT = T;
    using SelfT = FlatPool<DataT>;

    static constexpr int32_t skInvalidIndex = -1;
    static constexpr int32_t skChunkCapacity = 8192;
    static constexpr int32_t skChunkCapacityExponent = 13;  // 2^13 = 8192
    static constexpr int32_t skChunkCapacityMask = skChunkCapacity - 1;

    static constexpr int32_t skMaxChunkSize = 4096;
    static constexpr int32_t skChunkSizeMask = skMaxChunkSize - 1;
    static constexpr int32_t skMaxIndex = SelfT::skChunkCapacity * SelfT::skMaxChunkSize - 1;
    // sizeof(DataT*)*8 = 64 ==>cacheline size
    static constexpr int32_t skChunkIndexUpperBoundary = 8;

    template <class U>
    struct rebind {
        using other = FlatPool<U>;
    };

    struct Chunk {
        using SuperT = FlatPool<T>;
        using DataT = typename SuperT::DataT;

        static constexpr int32_t skSize = SuperT::skChunkCapacity;
        static constexpr int32_t skOffsetMask = ~(Chunk::skSize - 1);
        static constexpr int32_t skMask = SuperT::skChunkCapacityMask;
        static constexpr int32_t skDefaultPageSize = 4096;

        Chunk() = default;
        ~Chunk() {
            if (dataPtr_) {
                delete[] dataPtr_;
            }
        }

        constexpr int32_t capacity() { return Chunk::skSize; }
        void construct() { dataPtr_ = new DataT[Chunk::skSize]; }

        int32_t offset(DataT* data) const { return data - dataPtr_; }
        bool contains(DataT* data) const { return 0 == (offset(data) & Chunk::skOffsetMask); }

        DataT* begin() { return dataPtr_; }
        const DataT* begin() const { return dataPtr_; }

        DataT* end() { return dataPtr_ + Chunk::skSize; }
        const DataT* end() const { return dataPtr_ + Chunk::skSize; }

        DataT& operator[](uint32_t index) { return dataPtr_[index & Chunk::skMask]; }
        const DataT& operator[](uint32_t index) const { return dataPtr_[index & Chunk::skMask]; }

       private:
        DataT* dataPtr_ = nullptr;
    };
    using ChunkT = Chunk;

    struct ChunkInfo {
        const DataT* end_ = nullptr;
        const DataT* begin_ = nullptr;
        int32_t index_ = SelfT::skInvalidIndex;
        ChunkInfo() = default;
        ChunkInfo(const DataT* end, const DataT* begin = nullptr, int32_t index = SelfT::skInvalidIndex)
            : end_(end), begin_(begin), index_(index) {}
        inline bool operator<(const ChunkInfo& other) const { return end_ < other.end_; }
    } __attribute__((packed));

    FlatPool() = default;
    ~FlatPool() { chunkSet_.clear(); }

    FlatPool(FlatPool&& other) = delete;
    FlatPool(const FlatPool& other) = delete;

    FlatPool& operator=(FlatPool&& other) = delete;
    FlatPool& operator=(const FlatPool& other) = delete;

    DataT* alloc() {
        if (freeIndex_ == SelfT::skInvalidIndex) [[likely]] {
            ChunkT& chunkRef = getChunk();
            return &chunkRef[++latestIndex_];
        } else {
            DataT* data = &at(freeIndex_);
            freeIndex_ = *(reinterpret_cast<int32_t*>(data));
            return data;
        }
    }

    void dealloc(const DataT* data) {
        if (!data) return;

        int32_t index = getIndex(data);
        if (index ^ SelfT::skInvalidIndex) [[likely]] {
            *(reinterpret_cast<int32_t*>(const_cast<DataT*>(data))) = freeIndex_;
            freeIndex_ = index;
        }
    }

    std::pair<DataT*, uint32_t> allocate() {
        if (freeIndex_ == SelfT::skInvalidIndex) [[likely]] {
            ChunkT& chunkRef = getChunk();
            return {&chunkRef[++latestIndex_], latestIndex_};
        } else {
            const uint32_t index = freeIndex_;
            DataT* data = &at(freeIndex_);
            freeIndex_ = *(reinterpret_cast<int32_t*>(data));
            return {data, index};
        }
    }

    void deallocate(uint32_t index) {
        if (index <= latestIndex_) [[likely]] {
            *(reinterpret_cast<int32_t*>(&at(index))) = freeIndex_;
            freeIndex_ = index;
        }
    }

    constexpr size_t max_size() const { return SelfT::skChunkCapacity * SelfT::skMaxChunkSize; }

    inline DataT& at(uint32_t index) { return chunks_[index >> SelfT::skChunkCapacityExponent][index]; }
    inline const DataT& at(uint32_t index) const { return chunks_[index >> SelfT::skChunkCapacityExponent][index]; }

   private:
    int32_t getIndex(const DataT* data) {
        const ChunkInfo entry{data};
        auto it = chunkSet_.upper_bound(entry);
        if (it != chunkSet_.end() && data >= it->begin_) [[likely]] {
            return (data - it->begin_) | (it->index_ << SelfT::skChunkCapacityExponent);
        }
        return SelfT::skInvalidIndex;
    }

    ChunkT& getChunk() {
        const bool noNeedNewChunk = ((latestIndex_ + 1) & SelfT::skChunkCapacityMask);
        if (noNeedNewChunk) [[likely]] {
            return chunks_[latestChunkIndex_];
        } else {
            ChunkT& chunkRef = chunks_[++latestChunkIndex_];
            chunkRef.construct();
            chunkSet_.emplace(chunkRef.end(), chunkRef.begin(), latestChunkIndex_);
            return chunkRef;
        }
    }

   private:
    alignas(kDefaultCacheLineSize) int32_t freeIndex_ = SelfT::skInvalidIndex;
    alignas(kDefaultCacheLineSize) int32_t latestIndex_ = SelfT::skInvalidIndex;
    alignas(kDefaultCacheLineSize) int32_t latestChunkIndex_ = SelfT::skInvalidIndex;

    alignas(kDefaultCacheLineSize) std::set<ChunkInfo> chunkSet_{};
    alignas(kDefaultCacheLineSize) ChunkT chunks_[SelfT::skMaxChunkSize];
};
