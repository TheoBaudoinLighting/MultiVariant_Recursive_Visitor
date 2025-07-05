# MultiVariant Recursive Visitor for Modern C++

[![CI](https://img.shields.io/github/actions/workflow/status/TheoBaudoinLighting/MultiVariant_Recursive_Visitor/ci.yml?branch=main\&label=CI\&style=flat-square)](https://github.com/youruser/multivisitor/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp/17)
[![Coverage](https://img.shields.io/codecov/c/github/TheoBaudoinLighting/MultiVariant_Recursive_Visitor?style=flat-square)](https://codecov.io/gh/youruser/multivisitor)

> **Professional-grade, type-safe multi-dispatch and recursive visitation for C++17+**, designed to elegantly handle complex `std::variant` use cases.

---

## Features

* **Multi-variant dispatch:** visit N `std::variant` values simultaneously.
* **Tuple support:** clean dispatch over `std::tuple` of `std::variant`s with `multi_visit_tuple`.
* **MultiDispatcher:** auto-selects the best overload at compile time with `is_invocable`.
* **Header-only & dependency-free:** no Boost, no macros.
*  **Fully tested:** move-only types, constexpr, exception safety, large tuples, nested variants.

---

## Why use this?

C++ lacks native multi-pattern matching. `std::visit` only covers limited multi-variant cases and is cumbersome for tuples or dynamic packs.

This library abstracts that, letting you write expressive, type-safe code:

```cpp
std::variant<int, std::string> a = 42;
std::variant<char, bool> b = true;

multi_visit([](auto x, auto y) {
    std::cout << "Dispatch: " << typeid(x).name() << ", " << typeid(y).name() << "\n";
}, a, b);
```

Or over tuples:

```cpp
auto tuple = std::make_tuple(
    std::variant<int, double>{3.14},
    std::variant<char, bool>{'Z'}
);

multi_visit_tuple(
    overloaded{
        [](double d, char c) { std::cout << d << " and " << c; },
        [](auto...) { std::cout << "Fallback"; }
    },
    tuple
);
```

---

## Installation

This is a header-only library:

```cpp
#include "multi_visit.hpp"
```

Ready for CMake and simple to vendor into any project.

---

## Testing

Includes a full suite of GoogleTest cases covering:

* Recursive multi-visitation
* `tuple_transform` utilities
* `MultiDispatcher` overload selection
* Move-only types, exceptions, constexpr, nested structures

Run tests via:

```bash
mkdir build && cd build
cmake ..
make
ctest
```

---

## License

Released under the MIT License. See `LICENSE` for details.

---

## Contributing

PRs are welcome. Please open an issue for discussions, improvements, or extensions (e.g. C++20 concepts).

---

> "Making C++ pattern matching feel less like wizardry — and more like a natural language feature."
