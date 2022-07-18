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

/* CONTAINER REQUIREMENTS
 * All of those can be worked around by specializing `better_init::custom::??` for your container.
 * - Must have a `::value_type` typedef with the element type ()
 * - Must have a constructor from two iterators.
 */
/* FAILED DESIGN IDEAS
 * - Range elements with templated `operator T`.
 *     Rejected because MSVC couldn't handle it.
 * - Container hack that only modifies the allocator, not the element type.
 *     Rejected because:
 *     (A) libc++ has jank SFINAE on vector's iterator constructor, which uses `std::is_constructible` instead of allocator's `construct()`.
 *     (B) It becomes weird when the container rebinds the allocator.
 */

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

        // Don't want to include extra headers, so I roll my own typedefs.
        using size_t = decltype(sizeof(int));
        using ptrdiff_t = decltype((int *)nullptr - (int *)nullptr);
        using uintptr_t = size_t;
        static_assert(sizeof(uintptr_t) == sizeof(void *), "Internal error: Incorrect `uintptr_t` typedef, fix it.");

        template <typename T>
        T &&declval() noexcept; // Not defined.
    }

    // Customization points.
    namespace custom
    {
        // Whether to make the conversion operator of `init{...}` implicit.
        // `T` is the target container type.
        template <typename T, typename = void>
        struct allow_implicit_init : std::is_constructible<T, detail::any_init_list> {};

        // Because of a MSVC quirk (bug, probably?) we can't use a templated `operator T` for our range elements,
        // and must know exactly what we're converting to.
        template <typename T, typename = void>
        struct element_type {using type = typename T::value_type;};

        // How to construct `T` from a pair of iterators. Defaults to `T(begin, end, extra...)`.
        // Where `extra...` are the arguments passed to `.to<T>(...)`, or empty for a conversion operator.
        template <typename Void, typename T, typename Iter, typename ...P>
        struct construct
        {
            template <typename TT = T, std::enable_if_t<std::is_constructible<TT, Iter, Iter, P...>::value, int> = 0>
            constexpr T operator()(Iter begin, Iter end, P &&... params) const noexcept(std::is_nothrow_constructible<T, Iter, Iter, P...>::value)
            {
                // Don't want to include `<utility>` for `std::move` or `std::forward`.
                return T(static_cast<Iter &&>(begin), static_cast<Iter &&>(end), static_cast<P &&>(params)...);
            }
        };
    }
}

#if __has_include(BETTER_INIT_CONFIG)
#include BETTER_INIT_CONFIG
#endif

// Lets you change the identifier used for our initializer lists.
#ifndef BETTER_INIT_IDENTIFIER
#define BETTER_INIT_IDENTIFIER init
#endif

// The C++ standard version to assume.
// First, the raw date number.
#ifdef _MSC_VER
#define BETTER_INIT_CXX_STANDARD_DATE _MSVC_LANG // D:<
#else
#define BETTER_INIT_CXX_STANDARD_DATE __cplusplus
#endif
// Then, the actual version number.
#ifndef BETTER_INIT_CXX_STANDARD
#if BETTER_INIT_CXX_STANDARD_DATE >= 202002
#define BETTER_INIT_CXX_STANDARD 20
#elif BETTER_INIT_CXX_STANDARD_DATE >= 201703
#define BETTER_INIT_CXX_STANDARD 17
#elif BETTER_INIT_CXX_STANDARD_DATE >= 201402
#define BETTER_INIT_CXX_STANDARD 14
#elif BETTER_INIT_CXX_STANDARD_DATE >= 201103
#define BETTER_INIT_CXX_STANDARD 11
#else
#error C++98 is not supported.
#endif
#endif

// Whether we have mandatory copy elision or not.
#ifndef BETTER_INIT_HAVE_MANDATORY_COPY_ELISION
#if BETTER_INIT_CXX_STANDARD >= 17
#define BETTER_INIT_HAVE_MANDATORY_COPY_ELISION 1
#else
#define BETTER_INIT_HAVE_MANDATORY_COPY_ELISION 0
#endif
#endif

// Whether `std::iterator_traits` can guess the iterator category and various typedefs. This is a C++20 feature.
// If this is false, we're forced to include `<iterator>` to specify `std::random_access_iterator_tag`.
#ifndef BETTER_INIT_SMART_ITERATOR_TRAITS
#if BETTER_INIT_CXX_STANDARD >= 20
#define BETTER_INIT_SMART_ITERATOR_TRAITS 1
#else
#define BETTER_INIT_SMART_ITERATOR_TRAITS 0
#endif
#endif
#if !BETTER_INIT_SMART_ITERATOR_TRAITS
#include <iterator>
#endif

// Whether to allow braces: `init{...}`. Parentheses are always allowed: `init(...)`.
// Braces require CTAD to work.
// Note that here and elsewhere in C++, elements in braces are evaluated left-to-right, while in parentheses the evaluation order is unspecified.
#ifndef BETTER_INIT_ALLOW_BRACES
#if BETTER_INIT_CXX_STANDARD >= 17
#define BETTER_INIT_ALLOW_BRACES 1
#else
#define BETTER_INIT_ALLOW_BRACES 0
#endif
#endif

// How to stop the program when something bad happens.
#ifndef BETTER_INIT_ABORT
#ifdef _MSC_VER
#define BETTER_INIT_ABORT __debugbreak();
#else
#define BETTER_INIT_ABORT __builtin_trap();
#endif
#endif

// `[[nodiscard]]`, if available.
#ifndef BETTER_INIT_NODISCARD
#if BETTER_INIT_CXX_STANDARD >= 17
#define BETTER_INIT_NODISCARD [[nodiscard]]
#else
#define BETTER_INIT_NODISCARD
#endif
#endif

// Whether to allow the 'container hack', which means creating the container with a modified element type (and the respective allocator, etc),
// then converting it to the original container. This allows us to work with non-copyable types without relying on copy elision.
// This serves two purposes:
// * In C++14 and earlier, work around the lack of mandatory copy elision. It allows initialization of containers or non-movable elements.
// * In C++20 only in MSVC, work around a bug that causes `std::construct_at` (used by `std::vector` and others) to reject initialization that utilizes the mandatory copy elision.
//   This is issue https://github.com/microsoft/STL/issues/2620, fixed at June 20, 2022 by commit https://github.com/microsoft/STL/blob/3f203cb0d9bfde929a75eed877c228f88c0c7a46/stl/inc/xutility.
#ifndef BETTER_INIT_CONTAINER_HACK
#if BETTER_INIT_CXX_STANDARD < 17 || (defined(_MSC_VER) && BETTER_INIT_CXX_STANDARD >= 20)
#define BETTER_INIT_CONTAINER_HACK 1
#else
#define BETTER_INIT_CONTAINER_HACK 0
#endif
#endif
#if BETTER_INIT_CONTAINER_HACK
#include <memory> // For `std::allocator_traits`.
#endif

// When the allocator hack is used, we need a 'may alias' attribute to `reinterpret_cast` safely.
#ifndef BETTER_INIT_CONTAINER_HACK_MAY_ALIAS
#ifdef _MSC_VER
#define BETTER_INIT_CONTAINER_HACK_MAY_ALIAS // I've heard MSVC is fairly conservative with aliasing optimizations?
#else
#define BETTER_INIT_CONTAINER_HACK_MAY_ALIAS __attribute__((__may_alias__))
#endif
#endif

// If the container hack is enabled, apply it to all types, as opposed to move-only types.
#ifndef BETTER_INIT_CONTAINER_HACK_APPLY_UNCONDITIONALLY
#define BETTER_INIT_CONTAINER_HACK_APPLY_UNCONDITIONALLY 0
#endif


namespace better_init
{
    namespace detail
    {
        struct empty {};

        [[noreturn]] inline void abort() {BETTER_INIT_ABORT}

        // A boolean, artifically dependent on a type.
        template <typename T, bool X>
        struct dependent_value : std::integral_constant<bool, X> {};

        // Not using fold expressions avoid depending on C++17.
        constexpr bool all_of(std::initializer_list<bool> values)
        {
            for (bool x : values)
            {
                if (!x)
                    return false;
            }
            return true;
        }
        constexpr bool any_of(std::initializer_list<bool> values)
        {
            for (bool x : values)
            {
                if (x)
                    return true;
            }
            return false;
        }

        // Our reference classes inherit from this.
        struct ReferenceBase {};

        // This would be a lambda deep inside Reference, but constexpr lambdas are a C++17 feature.
        // `T` is the desired type, `U` is a forwarding reference type that `ptr` points to.
        template <typename T, typename U>
        constexpr T construct_from_elem(void *ptr)
        {
            // Don't want to include `<utility>` for `std::forward`.
            return T(static_cast<U &&>(*reinterpret_cast<std::remove_reference_t<U> *>(ptr)));
        }

        #if BETTER_INIT_CONTAINER_HACK
        // Constructs a `T` at `target`, passing a forwarding reference to `U` as an argument, which points to `ptr`.
        template <typename T, typename U>
        constexpr void container_hack_construct_from_elem_at(void *ptr, T *target)
        {
            ::new((void *)target) T(static_cast<U &&>(*reinterpret_cast<std::remove_reference_t<U> *>(ptr)));
        }
        #endif

        // Whether `T` is constructible from a pair of `Iter`s, possibly with extra arguments.
        template <typename Void, typename T, typename Iter, typename ...P>
        struct constructible_from_iters_helper : std::false_type {};
        template <typename T, typename Iter, typename ...P>
        struct constructible_from_iters_helper<decltype(void(custom::construct<void, T, Iter, P...>{}(declval<Iter &&>(), declval<Iter &&>(), declval<P &&>()...))), T, Iter, P...> : std::true_type {};
        template <typename T, typename Iter, typename ...P>
        struct constructible_from_iters : constructible_from_iters_helper<void, T, Iter, P...> {};

        template <typename T, typename Iter, typename ...P>
        struct nothrow_constructible_from_iters : std::integral_constant<bool, noexcept(custom::construct<void, T, Iter, P...>{}(declval<Iter &&>(), declval<Iter &&>(), declval<P &&>()...))> {};
    }

    #if BETTER_INIT_ALLOW_BRACES
    #define DETAIL_BETTER_INIT_CLASS_NAME BETTER_INIT_IDENTIFIER
    #else
    #define DETAIL_BETTER_INIT_CLASS_NAME helper
    #endif

    template <typename ...P>
    class BETTER_INIT_NODISCARD DETAIL_BETTER_INIT_CLASS_NAME
    {
      public:
        // Whether this list can be used to initialize a range of `T`s.
        template <typename T> static constexpr bool can_initialize_elem         = detail::all_of({std::is_constructible        <T, P &&>::value...});
        template <typename T> static constexpr bool can_nothrow_initialize_elem = detail::all_of({std::is_nothrow_constructible<T, P &&>::value...});

      private:
        template <typename T>
        class Reference : public detail::ReferenceBase
        {
            friend DETAIL_BETTER_INIT_CLASS_NAME;
            void *target = nullptr;
            detail::size_t index = 0;

            constexpr Reference() {}

          public:
            using owner = DETAIL_BETTER_INIT_CLASS_NAME;

            // Non-copyable.
            // The list creates and owns all its references, and exposes actual references to them.
            // This is because pre-C++20 iterator requirements force us to return actual references from `*`, and more importantly `[]`.
            Reference(const Reference &) = delete;
            Reference &operator=(const Reference &) = delete;

            // Conversion operators, for empty and non-empty ranges.

            template <typename U = T, std::enable_if_t<detail::dependent_value<U, sizeof...(P) == 0>::value, int> = 0>
            constexpr operator T() const noexcept(can_nothrow_initialize_elem<T>)
            {
                // Note: This is intentionally not a SFINAE check nor a `static_assert`, to support init from empty lists.
                detail::abort();
            }
            template <typename U = T, std::enable_if_t<detail::dependent_value<U, sizeof...(P) != 0>::value, int> = 0>
            constexpr operator T() const noexcept(can_nothrow_initialize_elem<T>)
            {
                constexpr T (*lambdas[])(void *) = {detail::construct_from_elem<T, P>...};
                return lambdas[index](target);
            }

            #if BETTER_INIT_CONTAINER_HACK
            // Those construct an object at the specified address, using the specified allocator.

            template <typename U = T, std::enable_if_t<detail::dependent_value<U, sizeof...(P) == 0>::value, int> = 0>
            constexpr void container_hack_construct_at(T *) const noexcept(can_nothrow_initialize_elem<T>)
            {
                detail::abort();
            }
            template <typename U = T, std::enable_if_t<detail::dependent_value<U, sizeof...(P) != 0>::value, int> = 0>
            constexpr void container_hack_construct_at(T *location) const noexcept(can_nothrow_initialize_elem<T>)
            {
                constexpr void (*lambdas[])(void *, T *) = {detail::container_hack_construct_from_elem_at<T, P>...};
                return lambdas[index](target, location);
            }
            #endif
        };

        template <typename T>
        class Iterator
        {
            friend class DETAIL_BETTER_INIT_CLASS_NAME;
            const Reference<T> *ref = nullptr;

          public:
            // Need `value_type` for the C++20 `std::iterator_traits` auto-detection to kick in.
            // Note that at least libstdc++'s category detection needs `value_type` to match the return type of `*`, except for cvref-qualifiers.
            // It's tempting to put `void` or some broken type here, to prevent extracting values from the range, which we don't want.
            // But that causes problems, and just `Reference` is enough, since it's non-copyable anyway.
            using value_type = Reference<T>;
            #if !BETTER_INIT_SMART_ITERATOR_TRAITS
            using iterator_category = std::random_access_iterator_tag;
            using reference = Reference<T>;
            using pointer = void;
            using difference_type = detail::ptrdiff_t;
            #endif

            using owner = DETAIL_BETTER_INIT_CLASS_NAME;

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
                return detail::uintptr_t(a.ref) < detail::uintptr_t(b.ref);
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
        // Could use `[[no_unique_address]]`, but it's our only member variable anyway.
        // Can't store `Reference`s here directly, because we can't use a templated `operator T` in our elements,
        // because it doesn't work correctly on MSVC (but not on GCC and Clang).
        std::conditional_t<sizeof...(P) == 0, detail::empty, void *[sizeof...(P) + (sizeof...(P) == 0)]> elems;

      public:
        // Whether this list can be used to initialize a range type `T`, with extra constructor parameters `P...`.
        template <typename T, typename ...Q> static constexpr bool can_initialize_range         = detail::constructible_from_iters        <T, Iterator<typename custom::element_type<T>::type>, Q...>::value && can_initialize_elem        <typename custom::element_type<T>::type>;
        template <typename T, typename ...Q> static constexpr bool can_nothrow_initialize_range = detail::nothrow_constructible_from_iters<T, Iterator<typename custom::element_type<T>::type>, Q...>::value && can_nothrow_initialize_elem<typename custom::element_type<T>::type>;

        // The constructor from a braced (or parenthesized) list.
        // No `[[nodiscard]]` because GCC 9 complains. Having it on the entire class should be enough.
        constexpr DETAIL_BETTER_INIT_CLASS_NAME(P &&... params) noexcept
            : elems{const_cast<void *>(static_cast<const void *>(&params))...}
        {}

        // The conversion functions below are `&&`-qualified as a reminder that your initializer elements can be dangling.

        // Implicit conversion to a container. Implicit-ness is only enabled when it has a `std::initializer_list` constructor.
        template <typename T, std::enable_if_t<can_initialize_range<T> && custom::allow_implicit_init<T>::value, int> = 0>
        BETTER_INIT_NODISCARD constexpr operator T() const && noexcept(can_nothrow_initialize_range<T>)
        {
            // Don't want to include `<utility>` for `std::move`.
            return static_cast<const DETAIL_BETTER_INIT_CLASS_NAME &&>(*this).to<T>();
        }
        // Explicit conversion to a container.
        template <typename T, std::enable_if_t<can_initialize_range<T>, int> = 0>
        BETTER_INIT_NODISCARD constexpr explicit operator T() const && noexcept(can_nothrow_initialize_range<T>)
        {
            // Don't want to include `<utility>` for `std::move`.
            return static_cast<const DETAIL_BETTER_INIT_CLASS_NAME &&>(*this).to<T>();
        }

        // Conversion to a container with extra arguments (such as an allocator).
        template <typename T, typename ...Q, std::enable_if_t<can_initialize_range<T, Q...> && sizeof...(P) == 0, int> = 0>
        BETTER_INIT_NODISCARD constexpr T to(Q &&... extra_args) const && noexcept(can_nothrow_initialize_range<T, Q...>)
        {
            using elem_type = typename custom::element_type<T>::type;
            return custom::construct<void, T, Iterator<elem_type>, Q...>{}(Iterator<elem_type>{}, Iterator<elem_type>{}, static_cast<Q &&>(extra_args)...);
        }
        // Conversion to a container with extra arguments (such as an allocator).
        template <typename T, typename ...Q, std::enable_if_t<can_initialize_range<T, Q...> && sizeof...(P) != 0, int> = 0>
        BETTER_INIT_NODISCARD constexpr T to(Q &&... extra_args) const && noexcept(can_nothrow_initialize_range<T, Q...>)
        {
            using elem_type = typename custom::element_type<T>::type;

            // Could use `std::array`, but want to use less headers.
            // Must store `Reference`s here, because `std::random_access_iterator` requires `operator[]` to return the same type as `operator*`,
            // and `LegacyForwardIterator` requires `operator*` to return an actual reference. If we don't have those here, we don't have anything for the references to point to.
            Reference<elem_type> refs[sizeof...(P)];
            for (detail::size_t i = 0; i < sizeof...(P); i++)
            {
                refs[i].target = elems[i];
                refs[i].index = i;
            }

            Iterator<elem_type> begin, end;
            begin.ref = refs;
            end.ref = refs + sizeof...(P);

            return custom::construct<void, T, Iterator<elem_type>, Q...>{}(begin, end, static_cast<Q &&>(extra_args)...);
        }
    };

    #if BETTER_INIT_ALLOW_BRACES
    // A deduction guide for the list class.
    template <typename ...P>
    DETAIL_BETTER_INIT_CLASS_NAME(P &&...) -> DETAIL_BETTER_INIT_CLASS_NAME<P...>;
    #else
    // A helper function to construct the list class.
    template <typename ...P>
    BETTER_INIT_NODISCARD constexpr DETAIL_BETTER_INIT_CLASS_NAME<P...> BETTER_INIT_IDENTIFIER(P &&... params) noexcept
    {
        return DETAIL_BETTER_INIT_CLASS_NAME<P...>(static_cast<P &&>(params)...);
    }
    #endif
}

using better_init::BETTER_INIT_IDENTIFIER;


// See the macro definition for details.
#if BETTER_INIT_CONTAINER_HACK

namespace better_init
{
    namespace container_hack
    {
        // This is used to modify container types. This is NOT a customization point; specialize `replace_type` instead.
        // `T` is the original container, or its template argument (recursively). `A` is the source element type, `B` is the modified element type.
        template <typename T, typename A, typename B, typename = void>
        struct basic_replace_type {using type = T;};
        // A customization point for `basic_replace_type`. You can specialize this.
        template <typename T, typename A, typename B, typename = void>
        struct replace_type : basic_replace_type<T, A, B> {};

        // Simple replacement.
        template <typename A, typename B>
        struct basic_replace_type<A, A, B> {using type = B;};
        // Simple replacement, with const.
        template <typename A, typename B>
        struct basic_replace_type<const A, A, B> {using type = const B;};
        // Recurse into template arguments.
        template <template <typename...> class X, typename ...Y, typename A, typename B>
        struct basic_replace_type<X<Y...>, A, B> {using type = X<typename replace_type<Y, A, B>::type...>;};

        // Whether `T` is an allocator class that we can replace.
        // `T` must be non-final, since we're going to inherit from it.
        // We could work around final allocators by making it a member and forwarding a bunch of calls to it, but that's too much work.
        // Also `T` can't be a specialization of our `modified_allocator<T>`.
        template <typename T, typename = void>
        struct is_allocator : std::false_type {};
        template <typename T>
        struct is_allocator<T, decltype(
            void(detail::declval<typename T::value_type>()),
            void(detail::declval<T &>().deallocate(detail::declval<T &>().allocate(size_t{}), size_t{}))
        )> : std::true_type {};
        // Specialize `basic_replace_type` for allocators. For those, we use use `rebind` instead of a simple template argument modification.
        template <typename T, typename A, typename B>
        struct basic_replace_type<T, A, B, std::enable_if_t<is_allocator<T>::value>> {using type = typename std::allocator_traits<T>::template rebind<B>::other;};

        // Whether `P...` holds a single type which is our reference class, possibly cvref-qualified.
        template <typename ...P>
        struct is_single_ref_param : std::false_type {};
        template <typename T>
        struct is_single_ref_param<T> : std::is_base_of<detail::ReferenceBase, std::remove_reference_t<T>> {};

        // Wraps `T` to make it constructible from our reference class without copy elision.
        template <typename T>
        struct basic_wrapper
        {
            alignas(T) unsigned char arr[sizeof(T)];

            T &get_ref()
            {
                return *
                #if BETTER_INIT_CXX_STANDARD >= 17
                std::launder
                #endif
                (
                    reinterpret_cast<T *>(arr)
                );
            }

            // Construct from our reference class.
            template <typename U, std::enable_if_t<std::is_base_of<detail::ReferenceBase, std::remove_reference_t<U>>::value &&
                U::owner::template can_initialize_elem<T>::value, int> = 0>
            constexpr basic_wrapper(U &&ref) noexcept(U::owner::template can_nothrow_initialize_elem<T>::value)
            {
                ref.container_hack_construct_at(arr);
            }

            // Wrap the existing constructors.
            template <typename ...P>
            constexpr basic_wrapper(P &&... params) noexcept(std::is_nothrow_constructible<T, P &&...>::value)
            {
                ::new((void *)arr) T(static_cast<P &&>(params)...);
            }

            // Wrap copy/move operations to make them non-template.
            constexpr basic_wrapper(const basic_wrapper &other) noexcept(std::is_nothrow_copy_constructible<T>::value)
            {
                ::new((void *)arr) T(other);
            }
            constexpr basic_wrapper(basic_wrapper &&other) noexcept(std::is_nothrow_move_constructible<T>::value)
            {
                ::new((void *)arr) T(static_cast<basic_wrapper &&>(other));
            }
            constexpr basic_wrapper &operator=(const basic_wrapper &other) noexcept(std::is_nothrow_copy_assignable<T>::value)
            {
                get_ref() = other;
            }
            constexpr basic_wrapper &operator=(basic_wrapper &&other) noexcept(std::is_nothrow_move_assignable<T>::value)
            {
                get_ref() = static_cast<basic_wrapper &&>(other);
            }

            // Destroy.
            ~basic_wrapper()
            {
                get_ref().~T();
            }
        };

        template <bool X>
        struct basic_wrapper_copy_constructible {};
        template <>
        struct basic_wrapper_copy_constructible<false>
        {
            basic_wrapper_copy_constructible() = default;
            basic_wrapper_copy_constructible(const basic_wrapper_copy_constructible &) = delete;
            basic_wrapper_copy_constructible(basic_wrapper_copy_constructible &&) = default;
            basic_wrapper_copy_constructible &operator=(const basic_wrapper_copy_constructible &) = default;
            basic_wrapper_copy_constructible &operator=(basic_wrapper_copy_constructible &&) = default;
        };
        template <bool X>
        struct basic_wrapper_move_constructible {};
        template <>
        struct basic_wrapper_move_constructible<false>
        {
            basic_wrapper_move_constructible() = default;
            basic_wrapper_move_constructible(const basic_wrapper_move_constructible &) = default;
            basic_wrapper_move_constructible(basic_wrapper_move_constructible &&) = delete;
            basic_wrapper_move_constructible &operator=(const basic_wrapper_move_constructible &) = default;
            basic_wrapper_move_constructible &operator=(basic_wrapper_move_constructible &&) = default;
        };
        template <bool X>
        struct basic_wrapper_copy_assignable {};
        template <>
        struct basic_wrapper_copy_assignable<false>
        {
            basic_wrapper_copy_assignable() = default;
            basic_wrapper_copy_assignable(const basic_wrapper_copy_assignable &) = default;
            basic_wrapper_copy_assignable(basic_wrapper_copy_assignable &&) = default;
            basic_wrapper_copy_assignable &operator=(const basic_wrapper_copy_assignable &) = delete;
            basic_wrapper_copy_assignable &operator=(basic_wrapper_copy_assignable &&) = default;
        };
        template <bool X>
        struct basic_wrapper_move_assignable {};
        template <>
        struct basic_wrapper_move_assignable<false>
        {
            basic_wrapper_move_assignable() = default;
            basic_wrapper_move_assignable(const basic_wrapper_move_assignable &) = default;
            basic_wrapper_move_assignable(basic_wrapper_move_assignable &&) = default;
            basic_wrapper_move_assignable &operator=(const basic_wrapper_move_assignable &) = default;
            basic_wrapper_move_assignable &operator=(basic_wrapper_move_assignable &&) = delete;
        };

        template <typename T>
        struct wrapper : basic_wrapper<T>,
            basic_wrapper_copy_constructible<std::is_copy_constructible<T>::value>,
            basic_wrapper_move_constructible<std::is_move_constructible<T>::value>,
            basic_wrapper_copy_assignable<std::is_copy_assignable<T>::value>,
            basic_wrapper_move_assignable<std::is_move_assignable<T>::value>
        {
            using wrapper::wrapper;
        };
    }

    namespace custom
    {
        template <typename T, typename Iter, typename ...P>
        struct construct<
            std::enable_if_t<
                #if !BETTER_INIT_CONTAINER_HACK_APPLY_UNCONDITIONALLY
                !std::is_move_constructible<T>::value &&
                #endif
                !std::is_same<
                    T,
                    container_hack::replace_type<
                        T,
                        typename custom::element_type<T>::type,
                        container_hack::wrapper<typename custom::element_type<T>::type>
                    >
                >::value
            >,
            T, Iter, P...
        >
        {
            using elem_type = typename element_type<T>::type;
            using fixed_elem_type = typename element_type<T>::type;
            using fixed_container = container_hack::replace_type<T, elem_type, fixed_elem_type>;

            constexpr T operator()(Iter begin, Iter end, P &&... params) const
            noexcept(noexcept(construct<void, fixed_container, Iter, P...>{}(detail::declval<Iter &&>(), detail::declval<Iter &&>(), detail::declval<P &&>()...)))
            {
                static_assert(sizeof(elem_type) == sizeof(fixed_elem_type) && alignof(elem_type) == alignof(fixed_elem_type), "Internal error: Our wrapper class didn't work correctly on an element type.");

                // Note that we intentionally `reinterpret_cast` (which requires `may_alias` and all that),
                // rather than memcpy-ing into the proper type. That's because the container might remember its own address.
                struct BETTER_INIT_CONTAINER_HACK_MAY_ALIAS alias_from {fixed_container value;};
                alias_from ret{construct<void, fixed_container, Iter, P...>{}(static_cast<Iter &&>(begin), static_cast<Iter &&>(end), static_cast<P &&>(params)...)};
                struct BETTER_INIT_CONTAINER_HACK_MAY_ALIAS alias_to {T value;};
                static_assert(sizeof(alias_from) == sizeof(alias_to) && alignof(alias_from) == alignof(alias_to), "Internal error: Our wrapper class didn't work correctly on a container.");
                return reinterpret_cast<alias_to &&>(ret).value;
            }
        };
    }
}
#endif
