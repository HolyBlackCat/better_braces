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

        // Don't want to include extra headers, so I roll my own typedefs.
        using size_t = decltype(sizeof(int));
        using ptrdiff_t = decltype((int *)nullptr - (int *)nullptr);
        using uintptr_t = size_t;
        static_assert(sizeof(uintptr_t) == sizeof(void *));

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
            template <typename TT = T, std::enable_if_t<std::is_constructible_v<TT, Iter, Iter, P...>, int> = 0>
            constexpr T operator()(Iter begin, Iter end, P &&... params) noexcept(std::is_nothrow_constructible_v<T, Iter, Iter, P...>)
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

// Whether `std::iterator_traits` can guess the various iterator typedefs. This is a C++20 feature.
// If this is false, we're forced to include `<iterator>` to specify `std::random_access_iterator_tag`.
#ifndef BETTER_INIT_SMART_ITERATOR_TRAITS
#if __cplusplus >= 202002
#define BETTER_INIT_SMART_ITERATOR_TRAITS 1
#else
#define BETTER_INIT_SMART_ITERATOR_TRAITS 0
#endif
#endif
#if !BETTER_INIT_SMART_ITERATOR_TRAITS
#include <iterator>
#endif

// MSVC's `std::construct_at()` has a broken SFINAE condition, interfering with construction of non-movable objects.
// This is issue https://github.com/microsoft/STL/issues/2620, fixed at June 20, 2022 by commit https://github.com/microsoft/STL/blob/3f203cb0d9bfde929a75eed877c228f88c0c7a46/stl/inc/xutility.
// We work around it by `reinterpret_cast`ing to a container with a modified allocator.
// 0 = disable hack, 1 = conditionally enable if the bug is detected, 2 = always enable (for testing).
#ifndef BETTER_INIT_ALLOCATOR_HACK
#ifdef _MSC_VER
#define BETTER_INIT_ALLOCATOR_HACK 1
#else
#define BETTER_INIT_ALLOCATOR_HACK 0
#endif
#endif
#if BETTER_INIT_ALLOCATOR_HACK > 0 && __cplusplus < 202002
#error BETTER_INIT_ALLOCATOR_HACK is only needed in C++20 and newer.
#endif
// When `BETTER_INIT_ALLOCATOR_HACK` is enabled, we need a 'may alias' attribute to `reinterpret_cast` safely.
#ifndef BETTER_INIT_ALLOCATOR_HACK_MAY_ALIAS
#ifdef _MSC_VER
#define BETTER_INIT_ALLOCATOR_HACK_MAY_ALIAS // I've heard MSVC is fairly conservative with aliasing optimizations?
#else
#define BETTER_INIT_ALLOCATOR_HACK_MAY_ALIAS __attribute__((__may_alias__))
#endif
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

        [[noreturn]] inline void abort() {BETTER_INIT_ABORT}

        // Whether `T` is constructible from a pair of `Iter`s, possibly with extra arguments.
        template <typename Void, typename T, typename Iter, typename ...P>
        struct constructible_from_iters_helper : std::false_type {};
        template <typename T, typename Iter, typename ...P>
        struct constructible_from_iters_helper<decltype(void(custom::construct<void, T, Iter, P...>{}(declval<Iter &&>(), declval<Iter &&>(), declval<P &&>()...))), T, Iter, P...> : std::true_type {};
        template <typename T, typename Iter, typename ...P>
        inline constexpr bool constructible_from_iters = constructible_from_iters_helper<void, T, Iter, P...>::value;

        template <typename T, typename Iter, typename ...P>
        struct nothrow_constructible_from_iters_helper : std::integral_constant<bool, noexcept(custom::construct<void, T, Iter, P...>{}(declval<Iter &&>(), declval<Iter &&>(), declval<P &&>()...))> {};
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
                    detail::abort();
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
            #if !BETTER_INIT_SMART_ITERATOR_TRAITS
            using iterator_category = std::random_access_iterator_tag;
            using reference = Reference<T>;
            using pointer = void;
            using difference_type = detail::ptrdiff_t;
            #endif

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
        // I know that `[[no_unique_address]]` is disabled in MSVC for now and they use a different attribute, but don't care because there are no other members here.
        // Can't store `Reference`s here directly, because we can't use a templated `operator T` in our elements,
        // because it doesn't work correctly on MSVC (but not on GCC and Clang).
        [[no_unique_address]] std::conditional_t<sizeof...(P) == 0, detail::empty, void *[sizeof...(P) + (sizeof...(P) == 0)]> elems;

      public:
        // Whether this list can be used to initialize a range type `T`, with extra constructor parameters `P...`.
        template <typename T, typename ...Q> static constexpr bool can_initialize_range         = detail::constructible_from_iters        <T, Iterator<typename custom::element_type<T>::type>, Q...> && can_initialize_elem        <typename custom::element_type<T>::type>;
        template <typename T, typename ...Q> static constexpr bool can_nothrow_initialize_range = detail::nothrow_constructible_from_iters<T, Iterator<typename custom::element_type<T>::type>, Q...> && can_nothrow_initialize_elem<typename custom::element_type<T>::type>;

        // The constructor from a braced (or parenthesized) list.
        [[nodiscard]] constexpr BETTER_INIT_IDENTIFIER(P &&... params) noexcept
            : elems{const_cast<void *>(static_cast<const void *>(&params))...}
        {}

        // The conversion functions below are `&&`-qualified as a reminder that your initializer elements can be dangling.

        // Implicit conversion to a container. Implicit-ness is only enabled when it has a `std::initializer_list` constructor.
        template <typename T, std::enable_if_t<can_initialize_range<T> && custom::allow_implicit_init<T>::value, int> = 0>
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
            using elem_type = typename custom::element_type<T>::type;

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

            return custom::construct<void, T, Iterator<elem_type>, Q...>{}(begin, end, static_cast<Q &&>(extra_args)...);
        }
    };

    template <typename ...P>
    BETTER_INIT_IDENTIFIER(P &&...) -> BETTER_INIT_IDENTIFIER<P...>;
}

using better_init::BETTER_INIT_IDENTIFIER;


// See the macro definition for details.
#if BETTER_INIT_ALLOCATOR_HACK > 0
#include <memory> // For `std::construct_at`, `std::allocator_traits`.

// If true, assume we have pre-C++20 allocators that define `.construct()` even though they don't need it. Blindly wrap all allocators.
// If false, assume that `.construct()` has the default behavior only if it's not defined (for the specific constructor parameters). Only wrap allocators that define it.
#ifndef BETTER_INIT_ALLOCATOR_HACK_ASSUME_BROKEN_CONSTRUCT_FUNC
#if defined(_MSC_VER) && (!defined(_HAS_DEPRECATED_ALLOCATOR_MEMBERS) || _HAS_DEPRECATED_ALLOCATOR_MEMBERS) // `<memory>` defines this.
#define BETTER_INIT_ALLOCATOR_HACK_ASSUME_BROKEN_CONSTRUCT_FUNC 1
#else
#define BETTER_INIT_ALLOCATOR_HACK_ASSUME_BROKEN_CONSTRUCT_FUNC 0
#endif
#endif

namespace better_init
{
    namespace detail
    {
        namespace allocator_hack
        {
            // Check if we're affected by the bug.
            #if BETTER_INIT_ALLOCATOR_HACK > 1
            inline constexpr bool compiler_has_broken_construct_at = true;
            #else
            struct construct_at_checker
            {
                constexpr construct_at_checker(int) {}
                construct_at_checker(const construct_at_checker &) = delete;
                construct_at_checker &operator=(const construct_at_checker &) = delete;
            };

            struct construct_at_checker_init
            {
                constexpr operator construct_at_checker() {return construct_at_checker(42);}
            };

            template <typename T, typename = void>
            struct compiled_has_broken_construct_at_helper : std::true_type {};
            template <typename T>
            struct compiled_has_broken_construct_at_helper<T, decltype(void(std::construct_at((T *)nullptr, construct_at_checker_init{})))> : std::false_type {};
            inline constexpr bool compiler_has_broken_construct_at = compiled_has_broken_construct_at_helper<construct_at_checker>::value;
            #endif


            template <typename Base> struct fixed_allocator;
            template <typename T> struct is_fixed_allocator : std::false_type {};
            template <typename T> struct is_fixed_allocator<fixed_allocator<T>> : std::true_type {};

            // Whether `T` is a broken allocator class.
            // We don't actually check 'broken-ness' (specialize this template if your allocator is sane).
            // `T` must be non-final, since we're going to inherit from it.
            // We could work around final allocators by making a member and forwarding a bunch of calls to it, but that's too much work.
            // Also `T` can't be a specialization of our `fixed_allocator<T>`.
            template <typename T, typename = void, typename = void>
            struct is_broken_allocator : std::false_type {};
            template <typename T>
            struct is_broken_allocator<T,
                decltype(
                    std::enable_if_t<
                        compiler_has_broken_construct_at &&
                        !std::is_final_v<T> &&
                        !is_fixed_allocator<std::remove_cvref_t<T>>::value
                    >(),
                    void(declval<typename T::value_type>()),
                    void(declval<T &>().deallocate(declval<T &>().allocate(size_t{}), size_t{}))
                ),
                // Check this last, because `std::allocator_traits` are not SFINAE-friendly to non-allocators.
                // This is in a separate template parameter, because GCC doesn't abort early enough otherwise.
                std::enable_if_t<
                    std::is_same_v<typename std::allocator_traits<T>::pointer, typename std::allocator_traits<T>::value_type *>
                >
            > : std::true_type {};

            // Whether `T` has an allocator template argument, satisfying `is_broken_allocator`.
            template <typename T, typename = void>
            struct has_broken_allocator : std::false_type {};
            template <template <typename...> class T, typename ...P, typename Void>
            struct has_broken_allocator<T<P...>, Void> : std::integral_constant<bool, (is_broken_allocator<P>::value || ...)> {};

            // If T satisfies `is_broken_allocator`, return our `fixed_allocator` for it. Otherwise return `T` unchanged.
            template <typename T, typename = void>
            struct replace_with_fixed_allocator {using type = T;};
            template <typename T>
            struct replace_with_fixed_allocator<T, std::enable_if_t<is_broken_allocator<T>::value>> {using type = fixed_allocator<T>;};

            // Apply `replace_with_fixed_allocator` to each template argument of `T`.
            template <typename T, typename = void>
            struct substitute_allocator {using type = T;};
            template <template <typename...> class T, typename ...P, typename Void>
            struct substitute_allocator<T<P...>, Void> {using type = T<typename replace_with_fixed_allocator<P>::type...>;};

            // Whether `.construct(ptr, P...)` should be wrapped, for allocator `T`.
            template <typename Void, typename T, typename ...P>
            struct should_wrap_construction : std::true_type {};
            #if !BETTER_INIT_ALLOCATOR_HACK_ASSUME_BROKEN_CONSTRUCT_FUNC
            template <typename T, typename ...P>
            struct should_wrap_construction<decltype(void(declval<T &>().construct((typename T::value_type *)nullptr, declval<P &&>()...))), T, P...> : std::false_type {};
            #endif

            template <typename Base>
            struct fixed_allocator : Base
            {
                // Allocator traits use this. We can't use the inherited one, since its typedef doesn't point to our own class.
                template <typename T>
                struct rebind
                {
                    using other = fixed_allocator<typename std::allocator_traits<Base>::template rebind_alloc<T>>;
                };

                // Solely for convenience. The rest of typedefs are inherited.
                using value_type = typename std::allocator_traits<Base>::value_type;
                using pointer = typename std::allocator_traits<Base>::pointer;

                // This is called by `construct()` if no workaround is needed.
                // Interestingly, clang complains if those are defined below `construct()`. Probably because they're mentioned in its `noexcept` specification.
                template <typename ...P>
                constexpr void construct_low(std::false_type, pointer &ptr, P &&... params)
                noexcept(noexcept(std::allocator_traits<Base>::construct(static_cast<Base &>(*this), static_cast<pointer &&>(ptr), static_cast<P &&>(params)...)))
                {
                    std::allocator_traits<Base>::construct(static_cast<Base &>(*this), static_cast<pointer &&>(ptr), static_cast<P &&>(params)...);
                }
                // This is called by `construct()` when the workaround is needed.
                template <typename ...P>
                constexpr void construct_low(std::true_type, pointer &ptr, P &&... params)
                noexcept(noexcept(::new(ptr) value_type(static_cast<P &&>(params)...)))
                {
                    ::new(ptr) value_type(static_cast<P &&>(params)...);
                }

                // Override `construct()` to do the right thing.
                template <typename ...P>
                constexpr void construct(pointer ptr, P &&... params)
                noexcept(noexcept(construct_low(should_wrap_construction<void, Base, P...>{}, ptr, static_cast<P &&>(params)...)))
                {
                    construct_low(should_wrap_construction<void, Base, P...>{}, ptr, static_cast<P &&>(params)...);
                }
            };
        }
    }

    namespace custom
    {
        template <typename T, typename Iter, typename ...P>
        struct construct<
            std::enable_if_t<
                !std::is_move_constructible_v<typename element_type<T>::type> &&
                detail::allocator_hack::has_broken_allocator<T>::value
            >,
            T, Iter, P...
        >
        {
            using elem_type = typename element_type<T>::type;

            using fixed_container = typename detail::allocator_hack::substitute_allocator<T>::type;

            constexpr T operator()(Iter begin, Iter end, P &&... params)
            noexcept(noexcept(construct<void, fixed_container, Iter, P...>{}(detail::declval<Iter &&>(), detail::declval<Iter &&>(), detail::declval<P &&>()...)))
            {
                fixed_container ret = construct<void, fixed_container, Iter, P...>{}(static_cast<Iter &&>(begin), static_cast<Iter &&>(end), static_cast<P &&>(params)...);
                // Note that we intentionally `reinterpret_cast`, rather than memcpy-ing into the proper type.
                // That's because the container might remember its own address.
                struct BETTER_INIT_ALLOCATOR_HACK_MAY_ALIAS aliased_type {T value;};
                return reinterpret_cast<aliased_type &&>(ret).value;
            }
        };
    }
}
#endif
