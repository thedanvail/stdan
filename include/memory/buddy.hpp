#pragma once

#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace stdan::memory {
    template<std::size_t TotalSize, std::size_t Depth>
        requires (TotalSize > 0)
            && (Depth < std::numeric_limits<unsigned long long>::digits - 1)
            && (TotalSize % (1ULL << Depth) == 0)
    class buddy_alloc {
    public:
        buddy_alloc()  = default;
        ~buddy_alloc() = default;
        buddy_alloc(const buddy_alloc&)  = delete;
        buddy_alloc(const buddy_alloc&&) = delete;
        buddy_alloc operator=(const buddy_alloc&) = delete;
        buddy_alloc operator=(const buddy_alloc&&) = delete;

        [[nodiscard]] void* alloc(
            std::size_t req,
            std::size_t alignment = alignof(std::max_align_t)) {
            if(req == 0 || req > TotalSize || alignment == 0
                || !std::has_single_bit(alignment) || alignment > TotalSize) {
                return nullptr;
            }

            std::size_t blockSize = TotalSize;
            while((blockSize / 2) >= req && blockSize != MIN_BLOCK_SIZE) {
                blockSize /= 2;
            }

            const std::size_t index = find_free_node(0, TotalSize, blockSize, alignment);
            if(index == NUM_NODES) { return nullptr; }

            mark_allocated(index);
            return pool_ + get_offset(index, blockSize);
        }

        void dealloc(void* ptr) {
            if(ptr == nullptr) { return; }
            std::size_t offset = static_cast<std::byte*>(ptr) - pool_;
            std::size_t index = find_node_index(offset);
            if(index < NUM_NODES) { unmark_allocated(index); }
        }

    private:
        static inline constexpr std::size_t NUM_NODES = (1ULL << (Depth + 1)) - 1;
        static inline constexpr std::size_t MIN_BLOCK_SIZE = TotalSize / (1ULL << Depth);
        static inline constexpr std::size_t POOL_ALIGNMENT =
            std::bit_floor(TotalSize) < alignof(std::max_align_t)
                ? alignof(std::max_align_t)
                : std::bit_floor(TotalSize);

        alignas(POOL_ALIGNMENT) std::byte pool_[TotalSize];
        std::bitset<NUM_NODES> tree_;
        std::bitset<NUM_NODES> allocated_;

        // Helper to get tree indices
        std::size_t get_left_child(std::size_t i) { return i * 2 + 1; }
        std::size_t get_right_child(std::size_t i) { return i * 2 + 2; }
        std::size_t get_parent(std::size_t i) { return (i - 1) / 2; }

        [[nodiscard]] std::size_t find_free_node(
            std::size_t idx,
            std::size_t size,
            std::size_t target_size,
            std::size_t alignment) {
            if(idx >= NUM_NODES || allocated_.test(idx)) { return NUM_NODES; }
            if(size == target_size) {
                const auto address = reinterpret_cast<std::uintptr_t>(
                    pool_ + get_offset(idx, size));
                return !tree_.test(idx) && address % alignment == 0 ? idx : NUM_NODES;
            }

            const std::size_t childBlockSize = size / 2;
            const std::size_t left = find_free_node(
                get_left_child(idx), childBlockSize, target_size, alignment);
            return left != NUM_NODES
                ? left
                : find_free_node(
                    get_right_child(idx), childBlockSize, target_size, alignment);
        }

        void mark_allocated(std::size_t idx) {
            allocated_.set(idx);
            while(true) {
                tree_.set(idx);
                if(idx == 0) { return; }
                idx = get_parent(idx);
            }
        }

        void unmark_allocated(std::size_t idx) {
            allocated_.reset(idx);
            tree_.reset(idx);

            while(idx != 0) {
                idx = get_parent(idx);
                tree_.set(idx,
                    allocated_.test(idx)
                        || tree_.test(get_left_child(idx))
                        || tree_.test(get_right_child(idx)));
            }
        }

        [[nodiscard]] std::size_t get_offset(std::size_t idx, std::size_t size) const {
            // level = log2(TotalSize / aBlockSize)
            std::size_t level = 0;
            std::size_t ratio = TotalSize / size;
            while (ratio >>= 1) {
                ++level;
            }

            std::size_t firstIndexInLevel = (1ULL << level) - 1;
            return (idx - firstIndexInLevel) * size;
        }

        [[nodiscard]] std::size_t find_node_index(std::size_t offset) {
            // Walk down from the root following the offset, returning the
            // first allocated node whose range starts at `offset`.
            std::size_t idx = 0;
            std::size_t blockSize = TotalSize;

            while (idx < NUM_NODES) {
                if (allocated_.test(idx) && get_offset(idx, blockSize) == offset) {
                    return idx;
                }

                blockSize /= 2;
                if (blockSize < MIN_BLOCK_SIZE) {
                    break;
                }

                // Decide whether to descend left or right based on offset
                std::size_t left = get_left_child(idx);
                if(offset < get_offset(left, blockSize) + blockSize) {
                    idx = left;
                }
                else {
                    idx = get_right_child(idx);
                }
            }

            return NUM_NODES;
        }

    };
} // namespace Memory
