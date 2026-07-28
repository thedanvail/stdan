#pragma once

#include "tmp_lists.hpp"

namespace stdan::bfs {

template<class Frontier>
struct next_front;

template<class... Nodes>
struct next_front<tmp_types::type_list<Nodes...>> {
    using type = tmp_types::concat_t<typename Nodes::children...>;
};

template<class Frontier>
struct bfs_order;

template<>
struct bfs_order<tmp_types::type_list<>> {
    using type = tmp_types::type_list<>;
};

template<class First, class... Rest>
struct bfs_order<tmp_types::type_list<First, Rest...>> {
private:
    using front = tmp_types::type_list<First, Rest...>;
    using next = typename next_front<front>::type;

public:
    using type = tmp_types::concat_t<
        front,
        typename bfs_order<next>::type
    >;
};

template<class Node>
using bfs_order_t = typename bfs_order<tmp_types::type_list<Node>>::type;

} // namespace stdan::bfs

