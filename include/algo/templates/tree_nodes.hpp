#pragma once

#include "tmp_lists.hpp"

namespace stdan::tree_nodes {

template<class T>
inline constexpr bool is_type_list = false;

template<class... Ts>
inline constexpr bool is_type_list<tmp_types::type_list<Ts...>> = true;

/// A tree node exposes its direct descendants as a `tmp_types::type_list`
/// named `children`. Tree algorithms assume the resulting graph is acyclic.
template<class Node>
concept tree_node = requires { typename Node::children; } && is_type_list<typename Node::children>;

template<int Value>
struct leaf {
    static constexpr int value = Value;
    using children = tmp_types::type_list<>;
};

template<class Tag, class... Children>
struct group {
    using tag = Tag;
    using children = tmp_types::type_list<Children...>;
};

struct root_tag;
struct branch_tag;

using tree = group<
    root_tag,
    group<branch_tag, leaf<1>, leaf<2>>,
    leaf<3>
>;

} // namespace stdan::tree_nodes

