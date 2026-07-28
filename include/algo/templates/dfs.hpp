#pragma once

#include "tmp_lists.hpp"

namespace stdan::dfs {

template<class Node>
struct dfs_order;

template<class Nodes>
struct dfs_children;

template<>
struct dfs_children<tmp_types::type_list<>> {
    using type = tmp_types::type_list<>;
};

template<class First, class... Rest>
struct dfs_children<tmp_types::type_list<First, Rest...>> {
    using type = tmp_types::concat_t<
        typename dfs_order<First>::type,
        typename dfs_children<tmp_types::type_list<Rest...>>::type
    >;
};

template<class Node>
struct dfs_order {
    using type = tmp_types::concat_t<
        tmp_types::type_list<Node>,
        typename dfs_children<typename Node::children>::type
    >;
};

template<class Node>
using dfs_order_t = typename dfs_order<Node>::type;

} // namespace stdan::dfs

