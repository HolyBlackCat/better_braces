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

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <utility> // At least for `std::swap`.

// This file is included by this header automatically, if it exists.
// Put your customizations here.
#ifndef BETTER_INIT_CONFIG
#define BETTER_INIT_CONFIG "better_init_config.hpp"
#endif

namespace better_init
{
    namespace detail
    {
        struct any_init_list
        {
            template <typename T>
            constexpr operator std::initializer_list<T>() const noexcept; // Not defined.
        };

        template <typename T>
        struct basic_range_traits
        {
            // Whether to make the conversion operator of `init{...}` implicit.
            static constexpr bool implicit = std::is_constructible_v<T, detail::any_init_list>;

            // How to construct `T` from a pair of iterators. Defaults to `T(begin, end)`.
            template <typename Iter>
            static constexpr T construct(Iter begin, Iter end) noexcept(std::is_nothrow_constructible_v<T, Iter, Iter>) requires std::is_constructible_v<T, Iter, Iter>
            {
                // Don't want to include `<utility>` for `std::move`.
                return T(static_cast<Iter &&>(begin), static_cast<Iter &&>(end));
            }
        };

        template <typename T>
        struct basic_elem_traits
        {
            // What type to cast `T` to before performing comparisons.
            // Must be `T`, possibly with cvref-qualifiers changed.
            using comparable_type = std::remove_reference_t<T> &;
        };
    }

    // Customization points.
    namespace custom
    {
        // Customizes the behavior of `init::to()` and the implicit conversion to a container.
        template <typename T> struct range_traits : detail::basic_range_traits<T> {};

        // Customizes the behavior of various operators on type-erased elements.
        template <typename T, typename = void> struct elem_traits : detail::basic_elem_traits<T> {};
    }
}

#if __has_include(BETTER_INIT_CONFIG)
#include BETTER_INIT_CONFIG
#endif

// Lets you change the identifier used for out initializer lists.
#ifndef BETTER_INIT_IDENTIFIER
#define BETTER_INIT_IDENTIFIER init
#endif

namespace better_init
{
    namespace detail
    {
        template <typename T, typename Iter>
        concept constructible_from_iters = requires
        {
            custom::range_traits<T>::construct(Iter{}, Iter{});
        };

        template <typename T, typename Iter>
        concept nothrow_constructible_from_iters = requires
        {
            { custom::range_traits<T>::construct(Iter{}, Iter{}) } noexcept;
        };

        // Returns the first type of a list.
        // Interestingly, making this a simple `using` doesn't work. Clang says: `Pack expansion used as argument for non-pack parameter of alias template`.
        template <typename T, typename...> struct first_type_helper {using type = T;};
        template <typename ...P> using first_type = typename first_type_helper<P...>::type;

        // Returns true if all types in a list are same.
        template <typename T, typename ...P> inline constexpr bool all_types_same = (std::is_same_v<T, P> && ...);

        template <typename T, typename ...P> inline constexpr bool all_swappable_with = (std::is_swappable_with_v<T, P> && ...);
        template <typename T, typename ...P> inline constexpr bool all_nothrow_swappable_with = (std::is_nothrow_swappable_with_v<T, P> && ...);
        template <typename ...P> inline constexpr bool all_swappable = (all_swappable_with<P, P...> && ...);
        template <typename ...P> inline constexpr bool all_nothrow_swappable = (all_nothrow_swappable_with<P, P...> && ...);

        inline constexpr auto perform_adl_swap = [](auto &&a, auto &&b) noexcept(std::is_nothrow_swappable_with_v<decltype(a), decltype(b)>) -> void
        requires std::is_swappable_with_v<decltype(a), decltype(b)>
        {
            using std::swap; // Enable ADL.
            swap(decltype(a)(a), decltype(b)(b));
        };

        namespace cmp
        {
            template <typename T>
            using comparable_type = typename custom::elem_traits<T>::comparable_type;

            #define DETAIL_BETTER_INIT_CMP_OPS(X) \
                X(eq,      ==) \
                X(neq,     !=) \
                X(less,    < ) \
                X(leq,     <=) \
                X(greater, > ) \
                X(greq,    >=)

            #define DETAIL_BETTER_INIT_CMP(name_, op_) \
                inline constexpr auto name_ = [](auto &&a, auto &&b) \
                noexcept(noexcept(static_cast<comparable_type<decltype(a)>>(a) op_ static_cast<comparable_type<decltype(b)>>(b))) \
                -> bool \
                requires std::is_same_v<bool, decltype(static_cast<comparable_type<decltype(a)>>(a) op_ static_cast<comparable_type<decltype(b)>>(b))> \
                {return static_cast<comparable_type<decltype(a)>>(a) op_ static_cast<comparable_type<decltype(b)>>(b);};

            DETAIL_BETTER_INIT_CMP_OPS(DETAIL_BETTER_INIT_CMP)
            #undef DETAIL_BETTER_INIT_CMP

            // Whether `T` can be compared with all of `P...` using `F()`.
            template <typename T, auto F, typename ...P>
            concept support_cmp_with = (requires(T &&t, P &&p)
            {
                F(static_cast<T &&>(t), static_cast<comparable_type<P &&>>(p));
            } && ...);
            template <typename T, auto F, typename ...P>
            concept support_nothrow_cmp_with = (requires(T &&t, P &&p)
            {
                { F(static_cast<T &&>(t), static_cast<comparable_type<P &&>>(p)) } noexcept;
            } && ...);

            // Whether `P...` can be compared among themselves using `F()`.
            template <auto F, typename ...P> concept support_cmp = (support_cmp_with<comparable_type<P &&>, F, P...> && ...);
            template <auto F, typename ...P> concept support_nothrow_cmp = (support_nothrow_cmp_with<comparable_type<P &&>, F, P...> && ...);
        }
    }

    template <typename ...P>
    class BETTER_INIT_IDENTIFIER
    {
      public:
        // Whether we only have lvalue references in the range, and no rvalue references.
        static constexpr bool lvalues_only = (std::is_lvalue_reference_v<P> && ...);

        // Whether this list can be used to initialize a range of `T`s.
        template <typename T> static constexpr bool can_initialize_elem         = (std::is_constructible_v        <T, P &&> && ...);
        template <typename T> static constexpr bool can_nothrow_initialize_elem = (std::is_nothrow_constructible_v<T, P &&> && ...);

        // Whether elements of this list can be assigned from `T`s.
        template <typename T> static constexpr bool can_assign_from_elem         = (std::is_assignable_v        <P &&, T &&> && ...);
        template <typename T> static constexpr bool can_nothrow_assign_from_elem = (std::is_nothrow_assignable_v<P &&, T &&> && ...);

      private:
        class Reference
        {
            friend BETTER_INIT_IDENTIFIER;
            void *target = nullptr;
            std::size_t index = 0;

            template <typename T>
            constexpr T to() const
            {
                if constexpr (sizeof...(P) == 0)
                {
                    return *(volatile T *)nullptr;
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

            template <typename R, auto F>
            constexpr R double_dispatch(Reference other) const
            {
                if constexpr (sizeof...(P) == 0)
                {
                    return *(volatile R *)nullptr;
                }
                else if constexpr (detail::all_types_same<P...>)
                {
                    using type = detail::first_type<P...>;
                    return F(to<type>(), other.to<type>());
                }
                else
                {
                    using func = R(void *, void *);
                    constexpr func *(*lambdas[])(std::size_t) = {
                        +[](std::size_t i)
                        {
                            using lhs = P &&;
                            constexpr func *sub_lambdas[] = {
                                +[](void *a, void *b) -> bool
                                {
                                    using rhs = P &&;
                                    return F(
                                        static_cast<lhs>(*reinterpret_cast<std::remove_reference_t<lhs> *>(a)),
                                        static_cast<rhs>(*reinterpret_cast<std::remove_reference_t<rhs> *>(b))
                                    );
                                }...
                            };
                            return sub_lambdas[i];
                        }...
                    };
                    return lambdas[index](other.index)(target, other.target);
                }
            }

          public:
            template <typename T> requires can_initialize_elem<T>
            constexpr operator T() const noexcept(can_nothrow_initialize_elem<T>)
            {
                return to<T>();
            }
            template <typename T> requires can_initialize_elem<T &&>
            constexpr operator T &&() const noexcept(can_nothrow_initialize_elem<T &&>)
            {
                return to<T &&>();
            }

            template <typename T> requires can_assign_from_elem<T>
            constexpr const Reference &operator=(T &&other) const noexcept(can_nothrow_assign_from_elem<T>)
            {
                if constexpr (sizeof...(P) == 0)
                {
                    *(volatile int *)nullptr = 0; // Crash.
                }
                else
                {
                    constexpr void (*lambdas[])(void *, T &&) = {
                        +[](void *ptr, T &&source)
                        {
                            // Don't want to include `<utility>` for `std::forward`.
                            static_cast<P &&>(*reinterpret_cast<std::remove_reference_t<P> *>(ptr)) = static_cast<T &&>(source);
                        }...
                    };
                    lambdas[index](target, static_cast<T &&>(other));
                }
                return *this;
            }

            // E.g. `std::sort` needs this.
            friend constexpr void swap(const Reference &a, const Reference &b) noexcept(detail::all_nothrow_swappable<P...>) requires detail::all_swappable<P...>
            {
                a.template double_dispatch<void, detail::perform_adl_swap>(b);
            }

            #define DETAIL_BETTER_INIT_CMP(name_, op_) \
                friend constexpr bool operator op_(Reference a, Reference b) \
                noexcept(detail::cmp::support_nothrow_cmp<detail::cmp::name_, P...>) \
                requires detail::cmp::support_cmp<detail::cmp::name_, P...> \
                {return a.template double_dispatch<bool, detail::cmp::name_>(b);}

            DETAIL_BETTER_INIT_CMP_OPS(DETAIL_BETTER_INIT_CMP)
            #undef DETAIL_BETTER_INIT_CMP
        };

        class Iterator
        {
            friend class BETTER_INIT_IDENTIFIER;
            const Reference *ref = nullptr;

          public:
            // C++20 `std::iterator_traits` needs this to auto-detect stuff.
            // Don't want to specify typedefs manually, because I'd need the iterator category tag, and I don't want to include `<iterator>`.
            using value_type = Reference;

            constexpr Iterator() noexcept {}

            // `LegacyForwardIterator` requires us to return an actual reference here.
            constexpr const Reference &operator*() const noexcept {return *ref;}

            // Can't be called using the operator notation, but can be helpful if somebody calls it using `.operator->()` to determine the "pointer type".
            constexpr Iterator operator->() const noexcept {return *this;}

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
                return std::uintptr_t(a.ref) < std::uintptr_t(b.ref);
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
            constexpr friend Iterator operator+(Iterator it, std::ptrdiff_t n) noexcept {it += n; return it;}
            constexpr friend Iterator operator+(std::ptrdiff_t n, Iterator it) noexcept {it += n; return it;}
            constexpr friend Iterator operator-(Iterator it, std::ptrdiff_t n) noexcept {it -= n; return it;}
            constexpr friend Iterator operator-(std::ptrdiff_t n, Iterator it) noexcept {it -= n; return it;}

            constexpr friend std::ptrdiff_t operator-(Iterator a, Iterator b) noexcept {return a.ref - b.ref;}

            constexpr Iterator &operator+=(std::ptrdiff_t n) noexcept {ref += n; return *this;}
            constexpr Iterator &operator-=(std::ptrdiff_t n) noexcept {ref -= n; return *this;}

            constexpr const Reference &operator[](std::ptrdiff_t i) const noexcept
            {
                return *(*this + i);
            }
        };

        // Could use `std::array`, but want to use less headers.
        // Must store `Reference`s here, because `std::random_access_iterator` requires `operator[]` to return the same type as `operator*`,
        // and `LegacyForwardIterator` requires `operator*` to return an actual reference. If we don't have those here, we don't have anything for the references to point to.
        struct Empty {};
        std::conditional_t<sizeof...(P) == 0, Empty, Reference[sizeof...(P)]> elems;

      public:
        // The element-wise constructor.
        [[nodiscard]] constexpr BETTER_INIT_IDENTIFIER(P &&... params) noexcept
        {
            std::size_t i = 0;
            ((elems[i].target = &params, elems[i].index = i, i++), ...);
        }

        // Iterators.
        // Those could be `&&`-qualified, but then we no longer satisfy `std::ranges::range`.
        [[nodiscard]] constexpr Iterator begin() const noexcept
        {
            Iterator ret;
            ret.ref = elems;
            return ret;
        }
        [[nodiscard]] constexpr Iterator end() const noexcept
        {
            Iterator ret;
            ret.ref = elems + sizeof...(P);
            return ret;
        }

        // Convert to a range.
        template <typename T> requires detail::constructible_from_iters<T, Iterator>
        [[nodiscard]] constexpr T to() const && noexcept(detail::nothrow_constructible_from_iters<T, Iterator>)
        {
            return custom::range_traits<T>::construct(begin(), end());
        }
        template <typename T> requires detail::constructible_from_iters<T, Iterator> && lvalues_only
        [[nodiscard]] constexpr T to() const & noexcept(detail::nothrow_constructible_from_iters<T, Iterator>)
        {
            return custom::range_traits<T>::construct(begin(), end());
        }

        // Implicitly convert to a range.
        template <typename T> requires detail::constructible_from_iters<T, Iterator>
        [[nodiscard]] constexpr explicit(!custom::range_traits<T>::implicit) operator T() const && noexcept(detail::nothrow_constructible_from_iters<T, Iterator>)
        {
            // Don't want to include `<utility>` for `std::move`.
            return static_cast<const BETTER_INIT_IDENTIFIER &&>(*this).to<T>();
        }
        template <typename T> requires detail::constructible_from_iters<T, Iterator> && lvalues_only
        [[nodiscard]] constexpr explicit(!custom::range_traits<T>::implicit) operator T() const & noexcept(detail::nothrow_constructible_from_iters<T, Iterator>)
        {
            return to<T>();
        }
    };

    template <typename ...P>
    BETTER_INIT_IDENTIFIER(P &&...) -> BETTER_INIT_IDENTIFIER<P...>;
}

using better_init::BETTER_INIT_IDENTIFIER;
