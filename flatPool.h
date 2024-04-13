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

    struct alignas(8) InnerData {
        DataT data_;
        int32_t index_ = SelfT::skInvalidIndex;
    };

    struct alignas(8) Chunk {
        using SuperT = FlatPool<InnerData>;

        static constexpr int32_t skSize = SuperT::skChunkCapacity;
        static constexpr int32_t skOffsetMask = ~(Chunk::skSize - 1);
        static constexpr int32_t skMask = SuperT::skChunkCapacityMask;
        static constexpr int32_t skDefaultPageSize = 4096;

        InnerData* data_ = nullptr;

        Chunk() = default;
        ~Chunk() {
            if (data_) {
                delete[] data_;
            }
        }

        constexpr int32_t capacity() { return Chunk::skSize; }
        void construct() { data_ = new InnerData[Chunk::skSize]; }

        inline InnerData& operator[](uint32_t index) { return data_[index & Chunk::skMask]; }
        inline const InnerData& operator[](uint32_t index) const { return data_[index & Chunk::skMask]; }
    };

    FlatPool() = default;
    ~FlatPool() = default;

    FlatPool(FlatPool&& other) = delete;
    FlatPool(const FlatPool& other) = delete;

    FlatPool& operator=(FlatPool&& other) = delete;
    FlatPool& operator=(const FlatPool& other) = delete;

    inline DataT* allocate() {
        if (freeIndex_ == SelfT::skInvalidIndex) [[likely]] {
            Chunk& chunkRef = getChunk();
            InnerData& dataRef = chunkRef[++latestIndex_];
            dataRef.index_ = latestIndex_;
            return &(dataRef.data_);
        } else {
            InnerData& innerData = at(freeIndex_);
            freeIndex_ = *(reinterpret_cast<int32_t*>(&innerData));
            return &(innerData.data_);
        }
    }

    // ? should safe-check
    inline void deallocate(const DataT* data) {
        if (!data) return;

        InnerData* innerData = const_cast<InnerData*>(reinterpret_cast<const InnerData*>(data));
        int32_t index = innerData->index_;
        if (index ^ SelfT::skInvalidIndex) [[likely]] {
            *(reinterpret_cast<int32_t*>(const_cast<DataT*>(data))) = freeIndex_;
            freeIndex_ = index;
        }
    }

    constexpr size_t max_size() const { return SelfT::skChunkCapacity * SelfT::skMaxChunkSize; }

   private:
    inline InnerData& at(uint32_t index) { return chunks_[index >> SelfT::skChunkCapacityExponent][index]; }
    inline const InnerData& at(uint32_t index) const { return chunks_[index >> SelfT::skChunkCapacityExponent][index]; }

    inline Chunk& getChunk() {
        const bool notNeedNewChunk = ((latestIndex_ + 1) & SelfT::skChunkCapacityMask);
        if (notNeedNewChunk) [[likely]] {
            return chunks_[latestChunkIndex_];
        } else {
            Chunk& chunkRef = chunks_[++latestChunkIndex_];
            chunkRef.construct();
            return chunkRef;
        }
    }

   private:
    int32_t freeIndex_ = SelfT::skInvalidIndex;
    int32_t latestIndex_ = SelfT::skInvalidIndex;
    int32_t latestChunkIndex_ = SelfT::skInvalidIndex;

    Chunk chunks_[SelfT::skMaxChunkSize];
};
