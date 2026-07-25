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

        void dealloc(void* ptr) {
            if(ptr == nullptr) { return; }
            std::size_t offset = static_cast<std::byte*>(ptr) - pool_;
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

        [[nodiscard]] std::size_t find_free_node(std::size_t idx, std::size_t size,
                                                  std::size_t target_size) {
            if(idx >= NUM_NODES || tree_.test(idx)) { return NUM_NODES; }
            if(size == target_size) { return idx; }

            const std::size_t childBlockSize = size / 2;
            const std::size_t left = find_free_node(get_left_child(idx), childBlockSize, target_size);
            return left != NUM_NODES
                ? left
                : find_free_node(get_right_child(idx), childBlockSize, target_size);
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

        void unmark_recursive(std::size_t idx) {
            clear_subtree(idx);

            if(idx == 0) { return; }

            std::size_t parent = get_parent(idx);
            std::size_t left = get_left_child(parent);
            std::size_t right = get_right_child(parent);

            // Coalesce - if both buddies are free, free the parent too.
            // We're not ICE after all.
            if (!tree_.test(left) && !tree_.test(right)) {
                unmark_recursive(parent);
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
                if (tree_.test(idx) && get_offset(idx, blockSize) == offset) {
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
