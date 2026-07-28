#pragma once

#include "tmp_lists.hpp"

// TODO: Documentation

namespace stdan::tree_nodes {

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

