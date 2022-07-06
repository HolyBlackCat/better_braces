# better_init

**A replacement for `std::initializer_list` that works with non-copyable types.**

---

For example, this doesn't work:

```cpp
std::vector<std::unique_ptr<int>> vec = {nullptr, std::make_unique<int>(42)};
```

...because `std::initializer_list` elements are `const` and can't be moved from.

This library provides a workaround:

```cpp
#include <better_init.hpp>
std::vector<std::unique_ptr<int>> vec = init{nullptr, std::make_unique<int>(42)};
```

In fact, non-movable types are also supported:
```cpp
#include <better_init.hpp>
std::vector<std::atomic_int> vec = init{1, 2, 3};
```

### Limitations

Since `init{...}` stores references to the elements, you should use it immediately, before the references become dangling.

### Usage

Single-header, no dependencies other than a few standard library headers.

A compiler with C++20 support is required.

`init{...}` is implicitly convertible to any class that's constructible from two iterators (our iterators are random-access), and has any `std::initializer_list` constructor.

If there's no `std::initializer_list` constructor, but a constructor from two iterators is still present, then the conversion `operator` is `explicit`.

`init{...}` has a `.to<T>()` function that's equivalent to explicitly casting it to `T`.

It's possible to customize the behavior of `operator T` and `.to()`, see the header file for more details.
