#pragma once

#include <bitset>
#include <cmath>
#include <cstddef>

namespace Memory
{
    template<std::size_t TotalSize, std::size_t Depth>
    class BuddyBlockAllocator
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
        size_t GetLeftChild(std::size_t i) { return i * 2 + 1; }
        size_t GetRightChild(std::size_t i) { return i * 2 + 2; }
        size_t GetParent(std::size_t i) { return (i - 1) / 2; }

        void MarkRecursive(std::size_t aIndex, bool aValue)
        {
            if (aIndex >= NumNodes) { return; }

            m_tree.set(aIndex, aValue);
            MarkRecursive(GetLeftChild(aIndex), aValue);
            MarkRecursive(GetRightChild(aIndex), aValue);
        }

        void UnmarkRecursive(std::size_t aIndex)
        {
            m_tree.set(aIndex, false);
            if(aIndex == 0) { return; }

            std::size_t parent = GetParent(aIndex);
            std::size_t left = GetLeftChild(parent);
            std::size_t right = GetRightChild(parent);

            // Coalesce - if both buddies are free, free the parent too.
            // We're not ICE after all.
            if (!m_tree.test(left) && !m_tree.test(right))
            {
                UnmarkRecursive(parent);
            }
        }

        [[nodiscard]] std::size_t GetOffset(std::size_t aIndex, std::size_t aBlockSize) const
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

        [[nodiscard]] std::size_t FindNodeIndex(std::size_t aOffset)
        {
            // Walk down from the root following the offset, returning the
            // first allocated node whose range starts at aOffset.
            std::size_t index = 0;
            std::size_t blockSize = TotalSize;

            while (index < NumNodes)
            {
                if (m_tree.test(index) && GetOffset(index, blockSize) == aOffset)
                {
                    return index;
                }

                blockSize /= 2;
                if (blockSize < MinBlockSize)
                {
                    break;
                }

                // Decide whether to descend left or right based on offset
                std::size_t left = GetLeftChild(index);
                if (aOffset < GetOffset(left, blockSize) + blockSize)
                {
                    index = left;
                }
                else
                {
                    index = GetRightChild(index);
                }
            }

            return NumNodes;
        }

    public:
        void* Allocate(std::size_t aRequest)
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
                    MarkRecursive(index, true);
                    return m_pool + GetOffset(index, blockSize);
                }

                blockSize /= 2;
                index = GetLeftChild(index);
            }

            return nullptr;
        }

        void Deallocate(void* aPtr)
        {
            if(aPtr == nullptr) { return; }
            std::size_t offset = static_cast<std::byte*>(aPtr) - m_pool;
            std::size_t index = FindNodeIndex(offset);
            if(index < NumNodes) { UnmarkRecursive(index); }
        }
    };

} // namespace Memory
