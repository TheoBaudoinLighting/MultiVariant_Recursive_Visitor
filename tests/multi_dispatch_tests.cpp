#include <gtest/gtest.h>
#include <variant>
#include <tuple>
#include <utility>
#include <type_traits>
#include <string>
#include <sstream>
#include <optional>
#include <memory>
#include <vector>

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
    constexpr auto try_dispatch(Args&&... args) const -> std::optional<int> {
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
    constexpr auto operator()(Args&&... args) const -> std::optional<int> {
        return try_dispatch<0>(std::forward<Args>(args)...);
    }
};

template<typename... Functors>
MultiDispatcher(Functors...) -> MultiDispatcher<Functors...>;

struct A { 
    int value; 
    bool operator==(const A& other) const { return value == other.value; }
};

struct B { 
    double value; 
    bool operator==(const B& other) const { return value == other.value; }
};

struct C { 
    char value; 
    bool operator==(const C& other) const { return value == other.value; }
};

struct D { 
    std::string value; 
    bool operator==(const D& other) const { return value == other.value; }
};

class MultiDispatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        call_count = 0;
        last_result = "";
    }
    
    static int call_count;
    static std::string last_result;
};

int MultiDispatchTest::call_count = 0;
std::string MultiDispatchTest::last_result = "";

TEST_F(MultiDispatchTest, IsVariantTrait) {
    using V1 = std::variant<int>;
    using V2 = std::variant<int, double, char>;
    EXPECT_TRUE(is_variant_v<V1>);
    EXPECT_TRUE(is_variant_v<V2>);
    EXPECT_FALSE(is_variant_v<int>);
    EXPECT_FALSE((is_variant_v<std::tuple<int, double>>));
    EXPECT_FALSE((is_variant_v<std::vector<int>>));
}

TEST_F(MultiDispatchTest, SingleVariantDispatch) {
    using V = std::variant<int, double, std::string>;
    V v1 = 42;
    
    int result = multi_visit([](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int>) {
            return val * 2;
        } else if constexpr (std::is_same_v<T, double>) {
            return static_cast<int>(val);
        } else {
            return static_cast<int>(val.length());
        }
    }, v1);
    
    EXPECT_EQ(result, 84);
}

TEST_F(MultiDispatchTest, TwoVariantsDispatch) {
    using V1 = std::variant<A, B>;
    using V2 = std::variant<B, C>;
    
    V1 v1 = A{10};
    V2 v2 = C{'X'};
    
    auto result = multi_visit([](auto&& x, auto&& y) {
        std::stringstream ss;
        using T1 = std::decay_t<decltype(x)>;
        using T2 = std::decay_t<decltype(y)>;
        
        if constexpr (std::is_same_v<T1, A> && std::is_same_v<T2, C>) {
            ss << "A=" << x.value << ",C=" << y.value;
        } else {
            ss << "Other";
        }
        return ss.str();
    }, v1, v2);
    
    EXPECT_EQ(result, "A=10,C=X");
}

TEST_F(MultiDispatchTest, ThreeVariantsDispatch) {
    using V = std::variant<int, double, char>;
    V v1 = 42;
    V v2 = 3.14;
    V v3 = 'Z';
    
    auto result = multi_visit([](auto a, auto b, auto c) -> int {
        if constexpr (std::is_same_v<decltype(a), int> && 
                      std::is_same_v<decltype(b), double> && 
                      std::is_same_v<decltype(c), char>) {
            return a + static_cast<int>(b) + static_cast<int>(c);
        } else {
            return -1;
        }
    }, v1, v2, v3);
    
    EXPECT_EQ(result, 42 + 3 + 90);
}

TEST_F(MultiDispatchTest, MultiDispatcherBasic) {
    auto dispatcher = MultiDispatcher{
        [](int i, double d) { return i + static_cast<int>(d); },
        [](double d, int i) { return static_cast<int>(d) * i; },
        [](auto, auto) { return -1; }
    };
    
    auto result1 = dispatcher(5, 3.7);
    auto result2 = dispatcher(2.5, 4);
    auto result3 = dispatcher("hello", 'c');
    
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 8);
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), 8);
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(result3.value(), -1);
}

TEST_F(MultiDispatchTest, MultiDispatcherWithVariants) {
    using V1 = std::variant<A, B>;
    using V2 = std::variant<B, C>;
    
    V1 v1 = A{42};
    V2 v2 = C{'Y'};
    
    auto dispatcher = MultiDispatcher{
        [&](A a, C c) { 
            call_count++;
            last_result = "A-C";
            return 1; 
        },
        [&](B b, B b2) { 
            call_count++;
            last_result = "B-B";
            return 2; 
        },
        [&](auto, auto) { 
            call_count++;
            last_result = "default";
            return 0; 
        }
    };
    
    auto result = multi_visit([&dispatcher](auto&& a, auto&& b) -> std::optional<int> {
        return dispatcher(a, b);
    }, v1, v2);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(last_result, "A-C");
}

TEST_F(MultiDispatchTest, MultiDispatcherNoMatch) {
    auto dispatcher = MultiDispatcher{
        [](int, int) { return 1; },
        [](double, double) { return 2; }
    };
    
    auto result = dispatcher("string", 'c');
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiDispatchTest, TupleOfVariantsBasic) {
    auto tuple = std::make_tuple(
        std::variant<int, double>{42},
        std::variant<char, bool>{'A'}
    );
    
    auto result = multi_visit_tuple([](int i, char c) {
        return std::string(1, c) + std::to_string(i);
    }, tuple);
    
    EXPECT_EQ(result, "A42");
}

TEST_F(MultiDispatchTest, TupleOfVariantsComplex) {
    auto tuple = std::make_tuple(
        std::variant<A, B>{B{3.14}},
        std::variant<C, D>{D{"test"}},
        std::variant<int, std::string>{123}
    );
    
    auto result = multi_visit_tuple(
        overloaded{
            [](B b, D d, int i) { 
                return d.value + std::to_string(i) + std::to_string(b.value);
            },
            [](auto...) { 
                return std::string("no match");
            }
        },
        tuple
    );
    
    EXPECT_EQ(result, "test1233.140000");
}

TEST_F(MultiDispatchTest, EmptyTuple) {
    auto empty_tuple = std::tuple<>();
    int counter = 0;
    
    multi_visit_tuple([&counter]() {
        counter++;
    }, empty_tuple);
    
    EXPECT_EQ(counter, 1);
}

TEST_F(MultiDispatchTest, LargeTuplePerformance) {
    auto large_tuple = std::make_tuple(
        std::variant<int, double>{1},
        std::variant<int, double>{2.0},
        std::variant<int, double>{3},
        std::variant<int, double>{4.0},
        std::variant<int, double>{5}
    );
    
    int sum = 0;
    multi_visit_tuple([&sum](auto... values) {
        ((sum += static_cast<int>(values)), ...);
    }, large_tuple);
    
    EXPECT_EQ(sum, 15);
}

TEST_F(MultiDispatchTest, MoveSemantics) {
    struct MoveOnly {
        std::unique_ptr<int> ptr;
        MoveOnly(int val) : ptr(std::make_unique<int>(val)) {}
        MoveOnly(MoveOnly&&) = default;
        MoveOnly& operator=(MoveOnly&&) = default;
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
    };
    
    using V = std::variant<MoveOnly, int>;
    V v = MoveOnly{42};
    
    auto result = multi_visit([](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, MoveOnly>) {
            return *val.ptr;
        } else {
            return val;
        }
    }, std::move(v));
    
    EXPECT_EQ(result, 42);
}

TEST_F(MultiDispatchTest, ConstexprSupport) {
    using V = std::variant<int, char>;
    
    constexpr auto test_func = []() {
        V v1 = 10;
        V v2 = 'A';
        return multi_visit([](int i, char c) {
            return i + static_cast<int>(c);
        }, v1, v2);
    };
    
    constexpr auto result = test_func();
    static_assert(result == 75);
    EXPECT_EQ(result, 75);
}

TEST_F(MultiDispatchTest, OverloadedPattern) {
    using V = std::variant<int, double, std::string>;
    V v = std::string("hello");
    
    auto visitor = overloaded{
        [](int i) { return i * 2; },
        [](double d) { return static_cast<int>(d * 3); },
        [](const std::string& s) { return static_cast<int>(s.length()); }
    };
    
    auto result = multi_visit(visitor, v);
    EXPECT_EQ(result, 5);
}

TEST_F(MultiDispatchTest, RecursiveVariants) {
    struct Tree;
    using TreePtr = std::shared_ptr<Tree>;
    using Node = std::variant<int, TreePtr>;
    
    struct Tree {
        Node left;
        Node right;
    };
    
    Node leaf1 = 10;
    Node leaf2 = 20;
    
    auto sum_visitor = overloaded{
        [](int val, int val2) { return val + val2; },
        [](auto, auto) { return -1; }
    };
    
    auto result = multi_visit(sum_visitor, leaf1, leaf2);
    EXPECT_EQ(result, 30);
}

TEST_F(MultiDispatchTest, ExceptionSafety) {
    struct ThrowingType {
        ThrowingType() = default;
        ThrowingType(const ThrowingType&) {
            throw std::runtime_error("copy error");
        }
    };
    
    using V = std::variant<int, ThrowingType>;
    V v1 = 42;
    V v2 = ThrowingType{};
    
    EXPECT_NO_THROW({
        multi_visit([](auto&&, auto&&) {}, v1, v2);
    });
}

TEST_F(MultiDispatchTest, ApplyWithIndexBasic) {
    auto tuple = std::make_tuple(1, 2.5, 'c');
    
    auto result = apply_with_index([](int i, double d, char c) {
        return i + static_cast<int>(d) + static_cast<int>(c);
    }, tuple);
    
    EXPECT_EQ(result, 1 + 2 + 99);
}

TEST_F(MultiDispatchTest, TupleTransform) {
    auto tuple = std::make_tuple(1, 2, 3);
    
    auto transformed = tuple_transform(tuple, [](auto x) { return x * 2; });
    
    EXPECT_EQ(std::get<0>(transformed), 2);
    EXPECT_EQ(std::get<1>(transformed), 4);
    EXPECT_EQ(std::get<2>(transformed), 6);
}

TEST_F(MultiDispatchTest, ComplexTypeHierarchy) {
    struct Base { virtual ~Base() = default; };
    struct Derived1 : Base { int x = 1; };
    struct Derived2 : Base { int x = 2; };
    
    using V = std::variant<Derived1, Derived2>;
    V v1 = Derived1{};
    V v2 = Derived2{};
    
    auto result = multi_visit([](auto& d1, auto& d2) {
        return d1.x + d2.x;
    }, v1, v2);
    
    EXPECT_EQ(result, 3);
}

TEST_F(MultiDispatchTest, NestedVariants) {
    using Inner = std::variant<int, char>;
    using Outer = std::variant<Inner, double>;
    
    Outer v = Inner{42};
    
    auto result = multi_visit([](auto&& val) -> int {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, Inner>) {
            return std::visit([](auto&& inner) -> int {
                using InnerT = std::decay_t<decltype(inner)>;
                if constexpr (std::is_same_v<InnerT, int>) {
                    return inner;
                } else {
                    return static_cast<int>(inner);
                }
            }, val);
        } else {
            return static_cast<int>(val);
        }
    }, v);
    
    EXPECT_EQ(result, 42);
}

TEST_F(MultiDispatchTest, VariadicTemplateExpansion) {
    auto test_expansion = [](auto... args) -> double {
        return (static_cast<double>(args) + ...);
    };
    
    using V = std::variant<int, double>;
    auto tuple = std::make_tuple(V{1}, V{2.0}, V{3});
    
    auto result = multi_visit_tuple(test_expansion, tuple);
    EXPECT_DOUBLE_EQ(result, 6.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}