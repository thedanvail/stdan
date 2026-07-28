#pragma once

namespace stdan::tmp_types {

template<class... Ts>
struct type_list {};

template<class... Lists>
struct concat;

template<>
struct concat<> {
    using type = type_list<>;
};

template<class List>
struct concat<List> {
    using type = List;
};

template<class... Left, class... Right, class... Rest>
struct concat<type_list<Left...>, type_list<Right...>, Rest...>
    : concat<type_list<Left..., Right...>, Rest...> {};

template<class... Lists>
using concat_t = typename concat<Lists...>::type;

} // namespace stdan::tmp_types

