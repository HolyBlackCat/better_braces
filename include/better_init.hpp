#pragma once

// better_init

// License: ZLIB

// Copyright (c) 2022 Egor Mikhailov
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include <initializer_list>
#include <type_traits>

// This file is included by this header automatically, if it exists.
// Put your customizations here.
// You can redefine various macros defined below (in the config file, or elsewhere),
// and specialize templates in `better_init::custom` (also in the config file, or elsewhere).
#ifndef BETTER_INIT_CONFIG
#define BETTER_INIT_CONFIG "better_init_config.hpp"
#endif

// CONTAINER REQUIREMENTS
// All of those can be worked around by specializing `better_init::custom::range_traits` for your container.
// * Must have a constructor from two iterators.
// * Must have a `::value_type` typedef with the element type ()

namespace better_init
{
    namespace detail
    {
        // Convertible to any `std::initializer_list<??>`.
        struct any_init_list
        {
            template <typename T>
            operator std::initializer_list<T>() const noexcept; // Not defined.
        };

        // Default implementation for `custom::range_traits`.
        // Override `custom::range_traits`, not this.
        template <typename T>
        struct basic_range_traits
        {
            // Whether to make the conversion operator of `init{...}` implicit.
            static constexpr bool implicit_init = std::is_constructible_v<T, detail::any_init_list>;

            // Because of a MSVC quirk (bug, probably?) we can't use a templated `operator T` for our range elements,
            // and must know exactly what we're converting to.
            using element_type = typename T::value_type;

            // How to construct `T` from a pair of iterators. Defaults to `T(begin, end)`.
            // Extra arguments can be appended at the end, if you pass them to `.to<T>(...)`.
            template <typename Iter, typename ...P, std::enable_if_t<std::is_constructible_v<T, Iter, Iter, P...>, int> = 0>
            static constexpr T construct(Iter begin, Iter end, P &&... params) noexcept(std::is_nothrow_constructible_v<T, Iter, Iter, P...>)
            {
                // Don't want to include `<utility>` for `std::move` or `std::forward`.
                return T(static_cast<Iter &&>(begin), static_cast<Iter &&>(end), static_cast<P &&>(params)...);
            }
        };
    }

    // Customization points.
    namespace custom
    {
        // Customizes the treatment of various containers.
        template <typename T, typename = void>
        struct range_traits : detail::basic_range_traits<T> {};
    }
}

#if __has_include(BETTER_INIT_CONFIG)
#include BETTER_INIT_CONFIG
#endif

// Lets you change the identifier used for out initializer lists.
#ifndef BETTER_INIT_IDENTIFIER
#define BETTER_INIT_IDENTIFIER init
#endif

// Should stop the program.
#ifndef BETTER_INIT_ABORT
#ifdef _MSC_VER
#define BETTER_INIT_ABORT __debugbreak();
#else
#define BETTER_INIT_ABORT __builtin_trap();
#endif
#endif

namespace better_init
{
    namespace detail
    {
        struct empty {};

        // Don't want to include extra headers, so I roll my own typedefs.
        using size_t = decltype(sizeof(int));
        using ptrdiff_t = decltype((int *)nullptr - (int *)nullptr);
        static_assert(sizeof(size_t) == sizeof(void *)); // We use it place of `std::uintptr_t` too.

        template <typename T>
        T &&declval() noexcept; // Not defined.

        // Whether `T` is constructible from a pair of `Iter`s, possibly with extra arguments.
        template <typename Void, typename T, typename Iter, typename ...P>
        struct constructible_from_iters_helper : std::false_type {};
        template <typename T, typename Iter, typename ...P>
        struct constructible_from_iters_helper<decltype(void(custom::range_traits<T>::construct(declval<Iter &&>(), declval<Iter &&>()))), T, Iter, P...> : std::true_type {};
        template <typename T, typename Iter, typename ...P>
        inline constexpr bool constructible_from_iters = constructible_from_iters_helper<void, T, Iter, P...>::value;

        template <typename T, typename Iter, typename ...P>
        struct nothrow_constructible_from_iters_helper : std::integral_constant<bool, noexcept(custom::range_traits<T>::construct(declval<Iter &&>(), declval<Iter &&>()))> {};
        template <typename T, typename Iter, typename ...P>
        inline constexpr bool nothrow_constructible_from_iters = nothrow_constructible_from_iters_helper<T, Iter, P...>::value;
    }

    template <typename ...P>
    class BETTER_INIT_IDENTIFIER
    {
      public:
        // Whether this list can be used to initialize a range of `T`s.
        template <typename T> static constexpr bool can_initialize_elem         = (std::is_constructible_v        <T, P &&> && ...);
        template <typename T> static constexpr bool can_nothrow_initialize_elem = (std::is_nothrow_constructible_v<T, P &&> && ...);

      private:
        template <typename T>
        class Reference
        {
            friend BETTER_INIT_IDENTIFIER;
            void *target = nullptr;
            detail::size_t index = 0;

            constexpr Reference() {}

          public:
            // Non-copyable.
            // The list creates and owns all its references, and exposes actual references to them.
            // This is because pre-C++20 iterator requirements force us to return actual references from `*`, and more importantly `[]`.
            Reference(const Reference &) = delete;
            Reference &operator=(const Reference &) = delete;

            constexpr operator T() const noexcept(can_nothrow_initialize_elem<T>)
            {
                if constexpr (sizeof...(P) == 0)
                {
                    // Note: This is intentionally not a SFINAE check nor a `static_assert`, to support init from empty lists.
                    BETTER_INIT_ABORT
                }
                else
                {
                    constexpr T (*lambdas[])(void *) = {
                        +[](void *ptr) -> T
                        {
                            // Don't want to include `<utility>` for `std::forward`.
                            return T(static_cast<P &&>(*reinterpret_cast<std::remove_reference_t<P> *>(ptr)));
                        }...
                    };
                    return lambdas[index](target);
                }
            }

            // Would also add `operator T &&`, but it confuses GCC (note, not libstdc++).
        };

        template <typename T>
        class Iterator
        {
            friend class BETTER_INIT_IDENTIFIER;
            const Reference<T> *ref = nullptr;

          public:
            // Need this for the C++20 `std::iterator_traits` auto-detection to kick in.
            // Note that at least libstdc++'s category detection needs this to match the return type of `*`, except for cvref-qualifiers.
            // It's tempting to put `void` or some broken type here, to prevent extracting values from the range, which we don't want.
            // But that causes problems, and just `Reference` is enough, since it's non-copyable anyway.
            using value_type = Reference<T>;
            // using iterator_category = std::random_access_iterator_tag;
            // using reference = Reference<T>;
            // using pointer = void;
            // using difference_type = detail::ptrdiff_t;

            constexpr Iterator() noexcept {}

            // `LegacyForwardIterator` requires us to return an actual reference here.
            constexpr const Reference<T> &operator*() const noexcept {return *ref;}

            // No `operator->`. This causes C++20 `std::iterator_traits` to guess `pointer_type == void`, which sounds ok to me.

            // Don't want to rely on `<compare>`.
            friend constexpr bool operator==(Iterator a, Iterator b) noexcept
            {
                return a.ref == b.ref;
            }
            friend constexpr bool operator!=(Iterator a, Iterator b) noexcept
            {
                return !(a == b);
            }
            friend constexpr bool operator<(Iterator a, Iterator b) noexcept
            {
                // Don't want to include `<functional>` for `std::less`, so need to cast to an integer to avoid UB.
                return detail::size_t(a.ref) < detail::size_t(b.ref);
            }
            friend constexpr bool operator> (Iterator a, Iterator b) noexcept {return b < a;}
            friend constexpr bool operator<=(Iterator a, Iterator b) noexcept {return !(b < a);}
            friend constexpr bool operator>=(Iterator a, Iterator b) noexcept {return !(a < b);}

            constexpr Iterator &operator++() noexcept
            {
                ++ref;
                return *this;
            }
            constexpr Iterator &operator--() noexcept
            {
                --ref;
                return *this;
            }
            constexpr Iterator operator++(int) noexcept
            {
                Iterator ret = *this;
                ++*this;
                return ret;
            }
            constexpr Iterator operator--(int) noexcept
            {
                Iterator ret = *this;
                --*this;
                return ret;
            }
            constexpr friend Iterator operator+(Iterator it, detail::ptrdiff_t n) noexcept {it += n; return it;}
            constexpr friend Iterator operator+(detail::ptrdiff_t n, Iterator it) noexcept {it += n; return it;}
            constexpr friend Iterator operator-(Iterator it, detail::ptrdiff_t n) noexcept {it -= n; return it;}
            // There's no `number - iterator`.

            constexpr friend detail::ptrdiff_t operator-(Iterator a, Iterator b) noexcept {return a.ref - b.ref;}

            constexpr Iterator &operator+=(detail::ptrdiff_t n) noexcept {ref += n; return *this;}
            constexpr Iterator &operator-=(detail::ptrdiff_t n) noexcept {ref -= n; return *this;}

            constexpr const Reference<T> &operator[](detail::ptrdiff_t i) const noexcept
            {
                return *(*this + i);
            }
        };

        // Could use `std::array`, but want to use less headers.
        // I know that `[[no_unique_address]]` is disabled in MSVC for now and they use a different attribute, but don't care because there are no other members here.
        // Can't store `Reference`s here directly, because we can't use a templated `operator T` in our elements,
        // because it doesn't work correctly on MSVC (but not on GCC and Clang).
        [[no_unique_address]] std::conditional_t<sizeof...(P) == 0, detail::empty, void *[sizeof...(P) + (sizeof...(P) == 0)]> elems;

      public:
        // Whether this list can be used to initialize a range type `T`, with extra constructor parameters `P...`.
        template <typename T, typename ...Q> static constexpr bool can_initialize_range         = detail::constructible_from_iters        <T, Iterator<T>, Q...> && can_initialize_elem        <typename custom::range_traits<T>::element_type>;
        template <typename T, typename ...Q> static constexpr bool can_nothrow_initialize_range = detail::nothrow_constructible_from_iters<T, Iterator<T>, Q...> && can_nothrow_initialize_elem<typename custom::range_traits<T>::element_type>;

        // The constructor from a braced (or parenthesized) list.
        [[nodiscard]] constexpr BETTER_INIT_IDENTIFIER(P &&... params) noexcept
            : elems{const_cast<void *>(static_cast<const void *>(&params))...}
        {}

        // The conversion functions below are `&&`-qualified as a reminder that your initializer elements can be dangling.

        // Implicit conversion to a container. Implicit-ness is only enabled when it has a `std::initializer_list` constructor.
        template <typename T, std::enable_if_t<can_initialize_range<T> && custom::range_traits<T>::implicit_init, int> = 0>
        [[nodiscard]] constexpr operator T() const && noexcept(can_nothrow_initialize_range<T>)
        {
            // Don't want to include `<utility>` for `std::move`.
            return static_cast<const BETTER_INIT_IDENTIFIER &&>(*this).to<T>();
        }
        // Explicit conversion to a container.
        template <typename T, std::enable_if_t<can_initialize_range<T>, int> = 0>
        [[nodiscard]] constexpr explicit operator T() const && noexcept(can_nothrow_initialize_range<T>)
        {
            // Don't want to include `<utility>` for `std::move`.
            return static_cast<const BETTER_INIT_IDENTIFIER &&>(*this).to<T>();
        }

        // Conversion to a container with extra arguments (such as an allocator).
        template <typename T, typename ...Q, std::enable_if_t<can_initialize_range<T, Q...>, int> = 0>
        [[nodiscard]] constexpr T to(Q &&... extra_args) const && noexcept(can_nothrow_initialize_range<T, Q...>)
        {
            using elem_type = typename custom::range_traits<T>::element_type;

            // Could use `std::array`, but want to use less headers.
            // Must store `Reference`s here, because `std::random_access_iterator` requires `operator[]` to return the same type as `operator*`,
            // and `LegacyForwardIterator` requires `operator*` to return an actual reference. If we don't have those here, we don't have anything for the references to point to.
            std::conditional_t<sizeof...(P) == 0, detail::empty, Reference<elem_type>[sizeof...(P) + (sizeof...(P) == 0)]> refs;
            Iterator<elem_type> begin, end;

            if constexpr (sizeof...(P) > 0)
            {
                for (detail::size_t i = 0; i < sizeof...(P); i++)
                {
                    refs[i].target = elems[i];
                    refs[i].index = i;
                }

                begin.ref = refs;
                end.ref = refs + sizeof...(P);
            }

            return custom::range_traits<T>::construct(begin, end, static_cast<Q &&>(extra_args)...);
        }
    };

    template <typename ...P>
    BETTER_INIT_IDENTIFIER(P &&...) -> BETTER_INIT_IDENTIFIER<P...>;
}

using better_init::BETTER_INIT_IDENTIFIER;
