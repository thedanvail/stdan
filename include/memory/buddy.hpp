#pragma once

#include <bitset>
#include <cmath>
#include <cstddef>

namespace stdan::memory {
    template<std::size_t TotalSize, std::size_t Depth>
    class buddy_alloc {
    public:
        buddy_alloc()  = default;
        ~buddy_alloc() = default;
        buddy_alloc(const buddy_alloc&)  = delete;
        buddy_alloc(const buddy_alloc&&) = delete;
        buddy_alloc operator=(const buddy_alloc&) = delete;
        buddy_alloc operator=(const buddy_alloc&&) = delete;

        [[nodiscard]] void* alloc(std::size_t req) {
            if(req == 0 || req > TotalSize) { return nullptr; }

            std::size_t blockSize = TotalSize;
            while((blockSize / 2) >= req && blockSize != MIN_BLOCK_SIZE) {
                blockSize /= 2;
            }

            const std::size_t index = find_free_node(0, TotalSize, blockSize);
            if(index == NUM_NODES) { return nullptr; }

            mark_recursive(index, true);
            return pool_ + get_offset(index, blockSize);
        }

        void dealloc(void* aPtr) {
            if(aPtr == nullptr) { return; }
            std::size_t offset = static_cast<std::byte*>(aPtr) - pool_;
            std::size_t index = find_node_index(offset);
            if(index < NUM_NODES) { unmark_recursive(index); }
        }

    private:
        static inline constexpr std::size_t NUM_NODES = (1ULL << (Depth + 1)) - 1;
        static inline constexpr std::size_t MIN_BLOCK_SIZE = TotalSize / (1ULL << Depth);

        std::byte pool_[TotalSize];
        std::bitset<NUM_NODES> tree_;

        // Helper to get tree indices
        std::size_t get_left_child(std::size_t i) { return i * 2 + 1; }
        std::size_t get_right_child(std::size_t i) { return i * 2 + 2; }
        std::size_t get_parent(std::size_t i) { return (i - 1) / 2; }

        [[nodiscard]] std::size_t find_free_node(std::size_t index, std::size_t blockSize,
                                                  std::size_t targetBlockSize) {
            if(index >= NUM_NODES || tree_.test(index)) { return NUM_NODES; }
            if(blockSize == targetBlockSize) { return index; }

            const std::size_t childBlockSize = blockSize / 2;
            const std::size_t left = find_free_node(get_left_child(index), childBlockSize, targetBlockSize);
            return left != NUM_NODES
                ? left
                : find_free_node(get_right_child(index), childBlockSize, targetBlockSize);
        }

        void mark_recursive(std::size_t idx, bool val) {
            if(idx >= NUM_NODES) { return; }

            tree_.set(idx, val);
            mark_recursive(get_left_child(idx), val);
            mark_recursive(get_right_child(idx), val);
        }

        void clear_subtree(std::size_t idx) {
            mark_recursive(idx, false);
        }

        void unmark_recursive(std::size_t aIndex) {
            clear_subtree(aIndex);

            if(aIndex == 0) { return; }

            std::size_t parent = get_parent(aIndex);
            std::size_t left = get_left_child(parent);
            std::size_t right = get_right_child(parent);

            // Coalesce - if both buddies are free, free the parent too.
            // We're not ICE after all.
            if (!tree_.test(left) && !tree_.test(right)) {
                unmark_recursive(parent);
            }
        }

        [[nodiscard]] std::size_t get_offset(std::size_t aIndex, std::size_t aBlockSize) const {
            // level = log2(TotalSize / aBlockSize)
            std::size_t level = 0;
            std::size_t ratio = TotalSize / aBlockSize;
            while (ratio >>= 1) {
                ++level;
            }

            std::size_t firstIndexInLevel = (1ULL << level) - 1;
            return (aIndex - firstIndexInLevel) * aBlockSize;
        }

        [[nodiscard]] std::size_t find_node_index(std::size_t aOffset) {
            // Walk down from the root following the offset, returning the
            // first allocated node whose range starts at aOffset.
            std::size_t index = 0;
            std::size_t blockSize = TotalSize;

            while (index < NUM_NODES) {
                if (tree_.test(index) && get_offset(index, blockSize) == aOffset) {
                    return index;
                }

                blockSize /= 2;
                if (blockSize < MIN_BLOCK_SIZE) {
                    break;
                }

                // Decide whether to descend left or right based on offset
                std::size_t left = get_left_child(index);
                if (aOffset < get_offset(left, blockSize) + blockSize) {
                    index = left;
                }
                else {
                    index = get_right_child(index);
                }
            }

            return NUM_NODES;
        }

    };

} // namespace Memory
