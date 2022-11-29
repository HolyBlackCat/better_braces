# ~ better_braces ~

**List-initialize containers with proper move semantics,<br/>
including containers of non-copyable elements, which otherwise don't support `std::initializer_list` constructors.**

[![tests badge](https://github.com/HolyBlackCat/better_braces/actions/workflows/tests.yml/badge.svg?branch=master)](https://github.com/HolyBlackCat/better_braces/actions?query=branch%3Amaster)<br/>
<kbd>[Try on gcc.godbolt.org][1]</kbd>

<details><summary>Table of contents</summary>
<p>

* [The problem](#the-problem)
* [The solution](#the-solution)
  * [More examples](#more-examples)
* [How do I use this library?](#how-do-i-use-this-library)
* [How does it work?](#how-does-it-work)
* [A detailed explanation](#a-detailed-explanation)
  * [The nature of `init`](#the-nature-of-init)
  * [Range vs non-range initialization](#range-vs-non-range-initialization)
  * [More on range initialization](#more-on-range-initialization)
  * [`.begin()`/`.end()`](#beginend)
* [Notes on compatibility](#notes-on-compatibility)
  * [MSVC and the allocator hack](#msvc-and-the-allocator-hack)
  * [C++14](#c14)
* [Using better_braces in your own libraries](#using-better_braces-in-your-own-libraries)
  * [The stable API](#the-stable-api)
  * [Supporting C++14](#supporting-c14)

</p>
</details>

<!-- To regenerate the table of contents, run:
grep -E '^##+ ' README.md | sed -E -e 's/^## /* /g' -e 's/^### /  * /g' -e 's/^#### /    * /g' | gawk '{$0 = gensub(/( *\* )(.*)/,"\\1[\\2]","g") "(#" gensub(/[^-_a-z0-9]/,"","g",gensub(/ /,"-","g",tolower(gensub(/ *\* /,"",1,$0)))) ")"; print}'
-->

## The problem

`std::initializer_list` stores `const` elements, so the elements can't be moved from it. This causes unnecessary copying, and compilation errors for non-copyable types.

For example, this doesn't work, since `unique_ptr` is non-copyable:

```cpp
std::vector<std::unique_ptr<int>> vec = {nullptr, std::make_unique<int>(42)};
```

Neither does this, since `std::atomic` is not even movable:
```cpp
std::vector<std::atomic_int> vec = {1, 2, 3};
```

## The solution

This library provides a workaround. Just include the header and add `init` before the braces:

```cpp
#include <better_braces.hpp>

std::vector<std::unique_ptr<int>> vec = init{nullptr, std::make_unique<int>(42)};
//                                      ^~~~
std::vector<std::atomic_int> vec = init{1, 2, 3};
//                                 ^~~~
```
This compiles and performs the initialization as you would expect.

In the first example, one could also use `.reserve()`+`.emplace_back()`, so `init{...}` just provides a prettier syntax.

In the second example, `.emplace_back()` wouldn't compile at all, because the type is non-movable.<br/>

### More examples

* **Initializing a map:** (or any nested container)

  ```cpp
  std::map<std::atomic_int, std::atomic_int> map = init{init{1,2}, init{3,4}};
  ```

  We support nested initializer lists, but the nested lists must use `init{...}` too, instead of just `{...}`.

* **Passing a custom allocator:** (or any extra constructor arguments)
  ```cpp
  std::vector<std::atomic_int, MyAllocator> vec = init{1, 2, 3}.and_with(MyAllocator(...));
  ```

* **Using `init{...}` as a list of references:**

  This requires all list elements to have exactly the same type.

  * With standard algorithms:

    ```cpp
    int x = 3, y = 2, z = 1;
    std::ranges::sort(init{x, y, z});
    std::cout << x << y << z << '\n'; // 123
    ```
    Or, pre-C++20:
    ```cpp
    auto list = init{x, y, z};
    std::sort(list.begin(), list.end());
    ```

  * With a `for` loop:
    ```cpp
    int x = 3, y = 2, z = 1;
    for (int &elem : init{x, y, z})
        elem++;
    ```
    This is equivalent to `for (int *elem : {&x, &y, &z}) (*elem)++;`, but with less annoying syntax.


## How do I use this library?

The library is header-only, with minimal standard library dependencies.

Just clone the repository, add the `include` directory to the header search path, and include `<better_braces.hpp>`.

To improve the build times, avoid using `init{...}` when it doesn't bring any benefit, i.e. with types that don't benefit from being moved (and are copyable), such as scalars, especially large arrays thereof.

We currently test on Clang 13+, GCC 9+, and MSVC v19.33, though earlier compiler versions might work as well. At least C++17 is recommended, but C++14 is supported [with some caveats](#c14).

If you don't like `init` in the global namespace, you can spell it as `better_braces::init`, and disable the short spelling by compiling with `-DBETTER_BRACES_SHORTHAND=0` (or by creating a file called `better_braces_config.hpp` with `#define BETTER_BRACES_SHORTHAND 0` in it, in a directory where `#include "..."` can find it).

Study `namespace custom` and the macros defined in `better_braces.hpp` for more customization capabilities.

## How does it work?

The container is constructed from a pair of iterators.

Those are custom iterators that deference to the provided initializers. The iterators are random-access, so containers should be initialized without reallocations.

A longer explanation is provided below.

## A detailed explanation

<sup>This describes behavior in C++17 and newer. C++14 has some quirks, see [C++14 compatibility](#supporting-c14).</sup>

### The nature of `init`

`init<P...>` is a class template. `P...` are deduced as the types of the elements passed to `init{...}`, in a forward reference manner (deducing non-references for rvalue elements, and lvalue references for lvalue elements).

`init` stores pointers to the elements. If at least one element is an rvalue (i.e. can become dangling), all operations become `&&`-qualified.

`init` has a templated conversion `operator`, which can be used to construct containers.

### Range vs non-range initialization

First, we determine if the target type looks like a container or not, i.e. if we should be constructing it from a pair of iterators. We call those container-like types "ranges".

Non-ranges are initialized directly with the list elements, instead of the iterators. Ranges are never initialized directly with the elements, so we avoid the direct-list-initialization fiasco.

> By the direct-list-initialization fiasco I mean the sad fact that even though `std::vector<int> x{2};` has a single element `2`, `std::vector<std::string> x{2};` instead has two empty string elements (because you can't construct a string from `2`, the element-wise initialization can't be performed and falls back to this).
>
> Normally you work around this by inserting `=` before the `{`, which bans the latter scenario, as the offending constructor is `explicit`.
>
> Our `init{...}` never performs the latter.

**A type is a "range" if has a `::value_type` typedef, and is not an aggregate.** We reject aggregates because `std::array` is one, and we don't want to initialize it with a pair of iterators. We also want to know this typedef for other reasons, see below.

**Ranges are initialized using `T(begin, end)`. Non-ranges are initialized using `T{elements...}`.** You can pass extra arguments to a range constructor by feeding them to `.and_with(...)`. Non-ranges don't accept `.and_with(...)` at all (except with no arguments, to simplify generic code).

The conversion `operator` from `init{...}` to a type can sometimes be **`explicit`**. For ranges, it happens when the type doesn't have a constructor accepting `std::initializer_list` (followed by the arguments passed to `.and_with()`, if any). For non-ranges, it happens when `void foo(Container); foo({...});` is ill-formed (most often this happens when the constructor is `explicit`).

### More on range initialization

Ranges are constructed from a pair of iterators, followed by the extra arguments passed to `.and_with()`, if any. The iterators are custom random-access iterators, so the container knows the target size immediately.

What the iterators deference to depends on whether the list is homogeneous.

An `init{...}` list is **"homogeneous"** if all its elements have the same type, and there's at least one element. Otherwise we call them **"heterogeneous"**. E.g. `init{1,2}` is homogeneous, but `init{1,2.0}` is not, `init{}` is not, and `int x;` `init{1,x}` is not (because the elements have different values categories: rvalue vs lvalue).

We call the sole element type of a homogeneous list the **"homogeneous type"**, it's always either an lvalue or rvalue reference.

Homogeneous list iterators dereference to its homogeneous type. Heterogeneous list iterators deference to a (const reference to a) helper objects that has an `operator T`, where `T` is the `::value_type` of the container.

### `.begin()`/`.end()`

Homogeneous lists expose `.begin()` and `.end()` as member functions. The iterators are random-access and dereference to the homogeneous type. Those are the same iterators that are used when constructing ranges.

Note that like all other member functions, `.begin()` and `.end()` become `&&`-qualified if the list contains at least one rvalue.

## Notes on compatibility

### MSVC and the allocator hack

MSVC is supported. MSVC v19.33 and earlier [have a bug](https://github.com/microsoft/STL/issues/2620) in C++20 mode and newer, which we work around with a hack.

Without the workaround, the bug prevents certain initialization scenarios, described below. The workaround fixes most of them (but not in `constexpr` contexts).

If the workaround causes problems, it can be disabled by defining `BETTER_BRACES_ALLOCATOR_HACK` to `0`. You can also tweak it for specific types by specializing templates in `namespace allocator_hack`.

The workaround involves `reinterpret_cast`ing the container to a different type with a modified allocator type. The hack only works if the allocator type can be found in the template arguments (possibly nested) of the container type. Otherwise it doesn't do anything, and you get a compilation error.

Note that, as far as I know, MSVC doesn't do strict-aliasing-based optimizations, so a careful `reinterpret_cast` should be safe. Since the hack is automatically disabled in newer MSVC versions, this shouldn't create problems in the future. In any case, there's a macro customization point that lets you inject something like `__attribute__((__may_alias__))` (if MSVC ever gets something like it), and in fact we already use it to test the allocator hack on other compilers.

The nature of the bug is that `std::construct_at()` (and by extension `std::allocator_traits<...>::construct()` of any allocator that doesn't define a custom `.constuct()` that doesn't rely on `std::construct_at()`, e.g. `std::allocator`) causes a SFINAE error when it's used to construct a non-movable type A from a type B that defines `operator A`. This is as if the mandatory copy elision wasn't considered. This doesn't necessarily mean that the mandatory copy elision doesn't happen in `std::construct_at`; only the SFINAE seems to be bugged and not the implementation.

For us, this breaks initialization of a container with a non-move-constructible element type<sup>1</sup> **if** `init{...}` is heterogeneous<sup>2</sup> or contains nested `init{...}` lists (or in general objects with a conversion `operator` to the element type).

> <sup>1</sup> Notably this includes maps with non-copyable keys, since the keys end up const and can't be moved.
>
> <sup>2</sup> That is, contains expressions of different types, e.g. `init{int(1), short(2)}` is heterogeneous, but `init{int(1), int(2)}` is not. An empty `init{}` is heterogeneous.

You can check at compile-time if the workaround is being used:
```cpp
// This is false if your compiler and its version look like they shouldn't be affected.
#if BETTER_BRACES_ALLOCATOR_HACK
// This is false if a compile-time test has determined that you're somehow not affected.
if constexpr (better_braces::detail::allocator_hack::enabled::value)
{
    // The hack is used.
}
#endif
```

### C++14

We support C++14 with some caveats:

* The syntax changes from `init{...}` to `init(...)`.

  Parentheses are supported in C++17+ too, but the braces should be preferred, because they look prettier, and because like everywhere else in C++, braces force the initializers to be evaluated left-to-right, while in parentheses the evaluation order is unspecified.

* Containers of non-movable types can only be initialized with homogeneous lists.<br/>
  (That is, all elements in `init(...)` must have the same type, and there must be at least one element.)

  E.g. `std::vector<std::atomic_int> a = init(1, 2);` is ok, but `= init(short(1), 2)` is not.

  This notably includes maps with non-copyable keys, since the keys end up const and can't be moved.

* Containers of non-movable types can't be initialized with nested `init(...)` lists.

  This, again, notably includes maps with non-copyable keys, since the keys end up const and can't be moved.

  For maps, you can use `std::make_pair()` instead of the nested `init(...)` lists, but note that it stores the elements by value, adding extra moves. You can use pairs of references to avoid those.

  The reason for this limitation is that a nested `init(...)` list tries to construct the element of the enclosing container, which involves returning it from `operator T` by value, which, in absence of the mandatory copy elision, requires a move constructor.

## Using better_braces in your own libraries

This primarily affects header-only libraries, or the libraries that are intended to be utilized directly as .cpp files, as opposed to being precompiled.

### The stable API

Don't spell `init` directly (even in a qualified way), since it can be renamed by the user with a configuration macro (`BETTER_INIT_IDENTIFIER`).

Instead of `init` and `better_braces::init`, use `BETTER_BRACES_INIT`.

Instead of `better_braces::type::init`, use `BETTER_BRACES_TYPE`.

### Supporting C++14

Normally `init` and `better_braces::init` are aliases for the class template `better_braces::type::init`.

But in C++14, since we don't have CTAD, `init` and `better_braces::init` are instead a function that returns a specialization of `better_braces::type::init`.

Because of that:

* Use `init(...)` instead of `init{...}`, since functions can't be called with braces. (Or rather, `BETTER_BRACES_INIT(...)`.)

  * Keep in mind that, as everywhere else in C++, the order of evaluation of parenthesized arguments is unspecified, while the braced elements are evaluated left-to-right.

* If you need to refer to the type, spell it as `better_braces::type::init`. (Or rather, `BETTER_BRACES_TYPE`.)

* C++14 doesn't have `std::is_aggregate` (no kidding). We consider `std::array` to be the only aggregate, **but** this only matters if the type also has a `::value_type` typedef. So if you want to initialize an aggregate that has this typedef, specialize `better_braces::details::is_aggregate` for it.


  [1]: https://godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:1,endLineNumber:21,positionColumn:1,positionLineNumber:21,selectionStartColumn:1,selectionStartLineNumber:21,startColumn:1,startLineNumber:21),source:'%23include+%3Chttps://raw.githubusercontent.com/HolyBlackCat/better_braces/master/include/better_braces.hpp%3E%0A%0A%23include+%3Catomic%3E%0A%23include+%3Ciostream%3E%0A%23include+%3Cmemory%3E%0A%23include+%3Cvector%3E%0A%0Aint+main()%0A%7B%0A++++//+std::vector%3Cstd::unique_ptr%3Cint%3E%3E+foo+%3D+%7Bnullptr,+std::make_unique%3Cint%3E(42)%7D%3B%0A++++std::vector%3Cstd::unique_ptr%3Cint%3E%3E+foo+%3D+init%7Bnullptr,+std::make_unique%3Cint%3E(42)%7D%3B%0A++++std::cout+%3C%3C+foo.at(0)+%3C%3C+!'%5Cn!'%3B%0A++++std::cout+%3C%3C+foo.at(1)+%3C%3C+%22+-%3E+%22+%3C%3C+*foo.at(1)+%3C%3C+!'%5Cn!'%3B%0A++++std::cout+%3C%3C+!'%5Cn!'%3B%0A++++%0A++++//+std::vector%3Cstd::atomic_int%3E+bar+%3D+%7B1,+2,+3%7D%3B%0A++++std::vector%3Cstd::atomic_int%3E+bar+%3D+init%7B1,+2,+3%7D%3B%0A++++for+(const+auto+%26elem+:+bar)%0A++++++++std::cout+%3C%3C+elem.load()+%3C%3C+!'%5Cn!'%3B%0A%7D%0A'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:50.6226993728496,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((g:!((h:compiler,i:(compiler:clang1500,deviceViewOpen:'1',filters:(b:'0',binary:'1',commentOnly:'0',demangle:'0',directives:'0',execute:'0',intel:'1',libraryCode:'1',trim:'0'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-std%3Dc%2B%2B20+-Wall+-Wextra+-pedantic-errors+-g+-fsanitize%3Dundefined,address+-D_GLIBCXX_DEBUG+-Wno-pragma-once-outside-header',selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1),l:'5',n:'0',o:'+x86-64+clang+15.0.0+(Editor+%231)',t:'0')),header:(),k:31.394545063431934,l:'4',m:50,n:'0',o:'',s:0,t:'0'),(g:!((h:output,i:(compilerName:'x86-64+gcc+(trunk)',editorid:1,fontScale:14,fontUsePx:'0',j:1,wrap:'1'),l:'5',n:'0',o:'Output+of+x86-64+clang+15.0.0+(Compiler+%231)',t:'0')),header:(),l:'4',m:50,n:'0',o:'',s:0,t:'0')),k:48.95267217704424,l:'3',n:'0',o:'',t:'0')),l:'2',n:'0',o:'',t:'0')),version:4
