#include <variant>
#include <tuple>
#include <utility>
#include <type_traits>
#include <iostream>

template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template<typename T>
struct is_variant : std::false_type {};

template<typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};

template<typename T>
inline constexpr bool is_variant_v = is_variant<T>::value;

template<typename Tuple, typename F, std::size_t... Is>
constexpr decltype(auto) apply_with_index_impl(F&& f, Tuple&& t, std::index_sequence<Is...>) {
    return std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t))...);
}

template<typename Tuple, typename F>
constexpr decltype(auto) apply_with_index(F&& f, Tuple&& t) {
    return apply_with_index_impl(
        std::forward<F>(f), 
        std::forward<Tuple>(t),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{}
    );
}

template<typename Visitor, typename Tuple, std::size_t I = 0>
struct VariantTupleVisitor {
    template<typename... Args>
    static constexpr decltype(auto) visit(Visitor&& visitor, Tuple&& tuple, Args&&... args) {
        if constexpr (I == std::tuple_size_v<std::remove_reference_t<Tuple>>) {
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

template<typename F, typename... Variants>
constexpr decltype(auto) multi_visit(F&& f, Variants&&... variants) {
    static_assert((is_variant_v<std::remove_reference_t<Variants>> && ...));
    return VariantTupleVisitor<F, std::tuple<Variants...>>::visit(
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

template<typename T, typename = void>
struct has_call_operator : std::false_type {};

template<typename T>
struct has_call_operator<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

template<typename F, typename... Args>
struct is_invocable_impl : std::false_type {};

template<typename F, typename... Args>
struct is_invocable_impl<F, std::enable_if_t<std::is_invocable_v<F, Args...>>, Args...> : std::true_type {};

template<typename Tuple, std::size_t... Is>
constexpr auto tuple_transform_impl(Tuple&& t, auto&& f, std::index_sequence<Is...>) {
    return std::make_tuple(f(std::get<Is>(std::forward<Tuple>(t)))...);
}

template<typename Tuple, typename F>
constexpr auto tuple_transform(Tuple&& t, F&& f) {
    return tuple_transform_impl(
        std::forward<Tuple>(t),
        std::forward<F>(f),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{}
    );
}

template<typename... Functors>
class MultiDispatcher {
    std::tuple<Functors...> functors;
    
    template<std::size_t I, typename... Args>
    constexpr decltype(auto) try_dispatch(Args&&... args) const {
        if constexpr (I < sizeof...(Functors)) {
            if constexpr (std::is_invocable_v<decltype(std::get<I>(functors)), Args...>) {
                return std::get<I>(functors)(std::forward<Args>(args)...);
            } else {
                return try_dispatch<I + 1>(std::forward<Args>(args)...);
            }
        } else {
            return std::nullopt;
        }
    }
    
public:
    constexpr explicit MultiDispatcher(Functors... fs) : functors(std::move(fs)...) {}
    
    template<typename... Args>
    constexpr decltype(auto) operator()(Args&&... args) const {
        return try_dispatch<0>(std::forward<Args>(args)...);
    }
};

template<typename... Functors>
MultiDispatcher(Functors...) -> MultiDispatcher<Functors...>;

struct A { int value; };
struct B { double value; };
struct C { char value; };

int main() {
    using V1 = std::variant<A, B>;
    using V2 = std::variant<B, C>;
    using V3 = std::variant<A, C>;
    
    V1 v1 = A{42};
    V2 v2 = C{'X'};
    V3 v3 = A{100};
    
    auto dispatcher = MultiDispatcher{
        [](A a, C c, A a2) { 
            std::cout << "A(" << a.value << "), C(" << c.value << "), A(" << a2.value << ")\n"; 
            return 1; 
        },
        [](B b, auto x, auto y) { 
            std::cout << "B avec autres types\n"; 
            return 2; 
        },
        [](auto x, auto y, auto z) { 
            std::cout << "Cas général\n"; 
            return 3; 
        }
    };
    
    multi_visit(dispatcher, v1, v2, v3);
    
    auto tuple_of_variants = std::make_tuple(
        std::variant<int, double>{3.14},
        std::variant<char, bool>{'Z'},
        std::variant<long, float>{42L}
    );
    
    multi_visit_tuple(
        overloaded{
            [](double d, char c, long l) { 
                std::cout << "double: " << d << ", char: " << c << ", long: " << l << "\n"; 
            },
            [](auto... args) { 
                std::cout << "Autres types: " << sizeof...(args) << " arguments\n"; 
            }
        },
        tuple_of_variants
    );
    
    return 0;
}