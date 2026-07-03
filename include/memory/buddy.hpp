#pragma once

#include <bitset>
#include <cmath>
#include <cstddef>

namespace stdan::memory 
{
    template<std::size_t TotalSize, std::size_t Depth>
    class buddy_alloc
    {

    private:
        static constexpr std::size_t NumNodes = (1ULL << (Depth + 1)) - 1;
        static constexpr std::size_t MinBlockSize = TotalSize / (1ULL << Depth);

        std::size_t m_totalSize;
        std::size_t m_minBlockSize;
        std::size_t m_numLeaves;

        std::byte m_pool[TotalSize];
        std::bitset<NumNodes> m_tree;

        // Helper to get tree indices
        size_t get_left_child(std::size_t i) { return i * 2 + 1; }
        size_t get_right_child(std::size_t i) { return i * 2 + 2; }
        size_t get_parent(std::size_t i) { return (i - 1) / 2; }

        void mark_recursive(std::size_t aIndex, bool aValue)
        {
            if (aIndex >= NumNodes) { return; }

            m_tree.set(aIndex, aValue);
            mark_recursive(get_left_child(aIndex), aValue);
            mark_recursive(get_right_child(aIndex), aValue);
        }

        void clear_subtree(std::size_t aIndex)
        {
            if (aIndex >= NumNodes) { return; }

            m_tree.set(aIndex, false);
            clear_subtree(get_left_child(aIndex));
            clear_subtree(get_right_child(aIndex));
        }

        void unmark_recursive(std::size_t aIndex)
        {
            if (aIndex >= NumNodes) { return; }

            clear_subtree(aIndex);

            if(aIndex == 0) { return; }

            std::size_t parent = get_parent(aIndex);
            std::size_t left = get_left_child(parent);
            std::size_t right = get_right_child(parent);

            // Coalesce - if both buddies are free, free the parent too.
            // We're not ICE after all.
            if (!m_tree.test(left) && !m_tree.test(right))
            {
                unmark_recursive(parent);
            }
        }

        [[nodiscard]] std::size_t get_offset(std::size_t aIndex, std::size_t aBlockSize) const
        {
            // level = log2(TotalSize / aBlockSize)
            std::size_t level = 0;
            std::size_t ratio = TotalSize / aBlockSize;
            while (ratio >>= 1)
            {
                ++level;
            }

            std::size_t firstIndexInLevel = (1ULL << level) - 1;
            return (aIndex - firstIndexInLevel) * aBlockSize;
        }

        [[nodiscard]] std::size_t find_node_index(std::size_t aOffset)
        {
            // Walk down from the root following the offset, returning the
            // first allocated node whose range starts at aOffset.
            std::size_t index = 0;
            std::size_t blockSize = TotalSize;

            while (index < NumNodes)
            {
                if (m_tree.test(index) && get_offset(index, blockSize) == aOffset)
                {
                    return index;
                }

                blockSize /= 2;
                if (blockSize < MinBlockSize)
                {
                    break;
                }

                // Decide whether to descend left or right based on offset
                std::size_t left = get_left_child(index);
                if (aOffset < get_offset(left, blockSize) + blockSize)
                {
                    index = left;
                }
                else
                {
                    index = get_right_child(index);
                }
            }

            return NumNodes;
        }

    public:
        void* alloc(std::size_t aRequest)
        {
            if(aRequest == 0 || aRequest > TotalSize) { return nullptr; }

            std::size_t blockSize = TotalSize;
            std::size_t index = 0;

            while(index < NumNodes)
            {
                if(m_tree.test(index)) { return nullptr; }

                // Stop if splitting further makes us go too small
                if((blockSize / 2) < aRequest || blockSize == MinBlockSize)
                {
                    mark_recursive(index, true);
                    return m_pool + get_offset(index, blockSize);
                }

                blockSize /= 2;
                index = get_left_child(index);
            }

            return nullptr;
        }

        void dealloc(void* aPtr)
        {
            if(aPtr == nullptr) { return; }
            std::size_t offset = static_cast<std::byte*>(aPtr) - m_pool;
            std::size_t index = find_node_index(offset);
            if(index < NumNodes) { unmark_recursive(index); }
        }
    };

} // namespace Memory
