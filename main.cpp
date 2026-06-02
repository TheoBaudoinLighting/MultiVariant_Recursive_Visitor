#include "multi_visit.hpp"

#include <iostream>
#include <tuple>
#include <variant>

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
