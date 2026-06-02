#pragma once

#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

template<typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace multi_visit_detail {

template<typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename Tuple, typename F, std::size_t... Is>
constexpr decltype(auto) apply_with_index_impl(F&& f, Tuple&& t, std::index_sequence<Is...>) {
    return std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t))...);
}

template<typename Visitor, typename Tuple, std::size_t I = 0>
struct VariantTupleVisitor {
    template<typename... Args>
    static constexpr decltype(auto) visit(Visitor&& visitor, Tuple&& tuple, Args&&... args) {
        if constexpr (I == std::tuple_size_v<remove_cvref_t<Tuple>>) {
            return std::forward<Visitor>(visitor)(std::forward<Args>(args)...);
        } else {
            return std::visit([&](auto&& val) -> decltype(auto) {
                return VariantTupleVisitor<Visitor, Tuple, I + 1>::visit(
                    std::forward<Visitor>(visitor),
                    std::forward<Tuple>(tuple),
                    std::forward<Args>(args)...,
                    std::forward<decltype(val)>(val)
                );
            }, std::get<I>(std::forward<Tuple>(tuple)));
        }
    }
};

template<typename Tuple, typename F, std::size_t... Is>
constexpr auto tuple_transform_impl(Tuple&& t, F&& f, std::index_sequence<Is...>) {
    return std::make_tuple(f(std::get<Is>(std::forward<Tuple>(t)))...);
}

} // namespace multi_visit_detail

template<typename T>
struct is_variant : std::false_type {};

template<typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};

template<typename T>
inline constexpr bool is_variant_v = is_variant<multi_visit_detail::remove_cvref_t<T>>::value;

template<typename F, typename Tuple>
constexpr decltype(auto) apply_with_index(F&& f, Tuple&& t) {
    return multi_visit_detail::apply_with_index_impl(
        std::forward<F>(f),
        std::forward<Tuple>(t),
        std::make_index_sequence<std::tuple_size_v<multi_visit_detail::remove_cvref_t<Tuple>>>{}
    );
}

template<typename F, typename... Variants>
constexpr decltype(auto) multi_visit(F&& f, Variants&&... variants) {
    static_assert((is_variant_v<Variants> && ...), "multi_visit expects std::variant arguments");

    return multi_visit_detail::VariantTupleVisitor<F, std::tuple<Variants...>>::visit(
        std::forward<F>(f),
        std::forward_as_tuple(std::forward<Variants>(variants)...)
    );
}

template<typename F, typename Tuple>
constexpr decltype(auto) multi_visit_tuple(F&& f, Tuple&& tuple) {
    return apply_with_index([&f](auto&&... variants) -> decltype(auto) {
        return multi_visit(std::forward<F>(f), std::forward<decltype(variants)>(variants)...);
    }, std::forward<Tuple>(tuple));
}

template<typename Tuple, typename F>
constexpr auto tuple_transform(Tuple&& t, F&& f) {
    return multi_visit_detail::tuple_transform_impl(
        std::forward<Tuple>(t),
        std::forward<F>(f),
        std::make_index_sequence<std::tuple_size_v<multi_visit_detail::remove_cvref_t<Tuple>>>{}
    );
}

template<typename... Functors>
class MultiDispatcher {
    overloaded<Functors...> functors;

public:
    constexpr explicit MultiDispatcher(Functors... fs) : functors{std::move(fs)...} {}

    template<typename... Args>
    constexpr auto operator()(Args&&... args) const -> std::optional<int> {
        if constexpr (std::is_invocable_r_v<int, const overloaded<Functors...>&, Args...>) {
            return functors(std::forward<Args>(args)...);
        } else {
            return std::nullopt;
        }
    }
};

template<typename... Functors>
MultiDispatcher(Functors...) -> MultiDispatcher<Functors...>;
