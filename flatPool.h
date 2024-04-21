#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <utility>
#include "util.h"

template <class T>
struct FlatPool final {
    using DataT = T;
    using SelfT = FlatPool<DataT>;

    static constexpr int32_t skMagicKey = 0x1A2B3C4D;
    static constexpr int32_t skInvalidIndex = -1;
    static constexpr int32_t skChunkCapacity = 8192;
    static constexpr int32_t skChunkCapacityExponent = 13;  // 2^13 = 8192
    static constexpr int32_t skChunkCapacityMask = skChunkCapacity - 1;

    static constexpr int32_t skMaxChunkSize = 4096;
    static constexpr int32_t skChunkSizeMask = skMaxChunkSize - 1;
    static constexpr int32_t skMaxIndex = SelfT::skChunkCapacity * SelfT::skMaxChunkSize - 1;

    template <class U>
    struct rebind {
        using other = FlatPool<U>;
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
            DataEntry& entry = chunkRef[++latestIndex_];
            entry.index_ = latestIndex_;
            entry.secret_ = (skMagicKey ^ latestIndex_);
            return &(entry.data_);
        } else {
            DataEntry& entry = at(freeIndex_);
            freeIndex_ = *(reinterpret_cast<int32_t*>(&entry));
            return &(entry.data_);
        }
    }

    inline void deallocate(const DataT* data) {
        if (!data) return;

        const DataEntry* entry = reinterpret_cast<const DataEntry*>(data);
        if ((entry->secret_ ^ skMagicKey) == entry->index_) [[likely]] {
            *(reinterpret_cast<int32_t*>(const_cast<DataT*>(data))) = freeIndex_;
            freeIndex_ = entry->index_;
        }
    }

    constexpr size_t max_size() const { return SelfT::skChunkCapacity * SelfT::skMaxChunkSize; }

   private:
    struct alignas(8) DataEntry {
        DataT data_;
        int32_t index_ = SelfT::skInvalidIndex;
        int32_t secret_ = SelfT::skInvalidIndex;
    };

    struct alignas(8) Chunk {
        using SuperT = FlatPool<DataEntry>;

        static constexpr int32_t skSize = SuperT::skChunkCapacity;
        static constexpr int32_t skMask = SuperT::skChunkCapacityMask;

        DataEntry* entry_ = nullptr;

        Chunk() = default;
        ~Chunk() {
            if (entry_) {
                delete[] entry_;
            }
        }

        constexpr int32_t capacity() { return Chunk::skSize; }
        void construct() { entry_ = new DataEntry[Chunk::skSize]; }

        inline DataEntry& operator[](uint32_t index) { return entry_[index & Chunk::skMask]; }
        inline const DataEntry& operator[](uint32_t index) const { return entry_[index & Chunk::skMask]; }
    };

    inline DataEntry& at(uint32_t index) { return chunks_[index >> SelfT::skChunkCapacityExponent][index]; }
    inline const DataEntry& at(uint32_t index) const { return chunks_[index >> SelfT::skChunkCapacityExponent][index]; }

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
