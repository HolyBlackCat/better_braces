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
#include <variant>

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

            // The type that gets copied when when you copy a type-erased element out of range. You get a variant of those.
            using extracted_type = std::remove_cvref_t<T>;
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
        enum class fwd {copy, move, forward};

        template <typename ...P> struct overload : P... {using P::operator()...;};
        template <typename ...P> overload(P &&...) -> overload<std::decay_t<P>...>;

        struct empty {};

        template <typename Tag, typename T>
        struct tagged_type
        {
            using tag = Tag;
            T value;
            template <typename U>
            constexpr tagged_type(U &&) noexcept(std::is_nothrow_constructible_v<T, U &&>) : value(std::forward<T>) {}
        };

        template <std::size_t N, typename F>
        void with_constexpr_index(std::size_t i, F &&func)
        {
            if constexpr (N == 0)
            {
                BETTER_INIT_ABORT
            }
            else
            {
                [&]<std::size_t ...I>(std::index_sequence<I...>)
                {
                    constexpr void (*lambdas[])(F &&) = {
                        +[](F &&func) -> void
                        {
                            std::forward<F>(func)(std::integral_constant<std::size_t, I>{});
                        }...
                    };
                    lambdas[i](std::forward<F>(func));
                }(std::make_index_sequence<N>{});
            }
        }

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
                X(eq,      ==, true ) \
                X(neq,     !=, true ) \
                X(less,    < , false) \
                X(leq,     <=, false) \
                X(greater, > , false) \
                X(greq,    >=, false)

            #define DETAIL_BETTER_INIT_CMP(name_, op_, is_eq_) \
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
        class Value;
        class Reference;

      public:
        // Whether we only have lvalue references in the range, and no rvalue references.
        static constexpr bool lvalues_only = (std::is_lvalue_reference_v<P> && ...);

        // Whether `T` is a special type that should be rejected by our generic templates.
        template <typename T>
        static constexpr bool is_special = std::is_same_v<std::remove_cvref_t<T>, Value> || std::is_same_v<std::remove_cvref_t<T>, Reference>;

        // Whether this list can be used to initialize a range of `T`s.
        template <typename T> static constexpr bool can_initialize_with_elem         = !is_special<T> && (std::is_constructible_v        <T, P &&> && ...);
        template <typename T> static constexpr bool can_nothrow_initialize_with_elem = !is_special<T> && (std::is_nothrow_constructible_v<T, P &&> && ...);

        // Whether elements of this list can be assigned from `T`s.
        template <typename T> static constexpr bool can_assign_elem_from         = !is_special<T> && (std::is_assignable_v        <P &&, T &&> && ...);
        template <typename T> static constexpr bool can_nothrow_assign_elem_from = !is_special<T> && (std::is_nothrow_assignable_v<P &&, T &&> && ...);

        // Whether elements can be mutually copy/move constructed/assigned.
        static constexpr bool can_copy_assign_elems         = (can_assign_elem_from        <const std::remove_reference_t<P> &> && ...);
        static constexpr bool can_nothrow_copy_assign_elems = (can_nothrow_assign_elem_from<const std::remove_reference_t<P> &> && ...);
        static constexpr bool can_move_assign_elems         = (can_assign_elem_from        <std::remove_reference_t<P> &&> && ...);
        static constexpr bool can_nothrow_move_assign_elems = (can_nothrow_assign_elem_from<std::remove_reference_t<P> &&> && ...);

        // Whether you can extract elements from range into iterator's `::value_type` (a wrapper for a variant).
        static constexpr bool can_copy_extract_elems         = (std::is_constructible_v        <typename custom::elem_traits<P>::extracted_type, const std::remove_reference_t<P> &> && ...);
        static constexpr bool can_nothrow_copy_extract_elems = (std::is_nothrow_constructible_v<typename custom::elem_traits<P>::extracted_type, const std::remove_reference_t<P> &> && ...);
        static constexpr bool can_move_extract_elems         = (std::is_constructible_v        <typename custom::elem_traits<P>::extracted_type, std::remove_reference_t<P> &&> && ...);
        static constexpr bool can_nothrow_move_extract_elems = (std::is_nothrow_constructible_v<typename custom::elem_traits<P>::extracted_type, std::remove_reference_t<P> &&> && ...);

        // Whether you can re-insert elements from iterator's `::value_type`.
        static constexpr bool can_copy_insert_elems         = (std::is_assignable_v        <P &, const std::remove_reference_t<typename custom::elem_traits<P>::extracted_type> &> && ...);
        static constexpr bool can_nothrow_copy_insert_elems = (std::is_nothrow_assignable_v<P &, const std::remove_reference_t<typename custom::elem_traits<P>::extracted_type> &> && ...);
        static constexpr bool can_move_insert_elems         = (std::is_assignable_v        <P &, std::remove_reference_t<typename custom::elem_traits<P>::extracted_type> &&> && ...);
        static constexpr bool can_nothrow_move_insert_elems = (std::is_nothrow_assignable_v<P &, std::remove_reference_t<typename custom::elem_traits<P>::extracted_type> &&> && ...);

      private:
        class Value
        {
            friend BETTER_INIT_IDENTIFIER;
            std::variant<std::monostate, typename custom::elem_traits<P>::extracted_type...> var;
        };

        class Reference
        {
            friend BETTER_INIT_IDENTIFIER;

            std::variant<std::monostate, detail::tagged_type<P, std::remove_reference_t<P> *>...> var;

            template <detail::fwd Mode, typename T>
            constexpr T detail_to() const
            {
                return std::visit(detail::overload{
                    [](std::monostate) -> T
                    {
                        return *(T *)nullptr;
                    },
                    []<typename U, typename V>(detail::tagged_type<U, V *> ptr) -> T
                    {
                        if constexpr (Mode == detail::fwd::copy)
                            return T(*ptr.value);
                        else if constexpr (Mode == detail::fwd::move)
                            return T(std::move(*ptr.value));
                        else
                            return T(std::forward<U>(*ptr.value));
                    },
                }, var);
            }

            template <auto F, bool HandleNulls>
            constexpr bool detail_compare(const Reference &other) const
            {
                return std::visit(detail::overload{
                    []<typename A, typename B>(A, B) -> bool requires std::is_same_v<A, std::monostate> || std::is_same_v<A, std::monostate>
                    {
                        if constexpr (HandleNulls)
                            return F(!std::is_same_v<A, std::monostate>, !std::is_same_v<B, std::monostate>);
                        else
                            BETTER_INIT_ABORT
                    },
                    []<typename UA, typename VA, typename UB, typename VB>(detail::tagged_type<UA, VA *> a, detail::tagged_type<UB, VB *> b) -> bool
                    {
                        return F(
                            static_cast<typename custom::elem_traits<UA>::comparable_type>(*a.value),
                            static_cast<typename custom::elem_traits<UB>::comparable_type>(*b.value)
                        );
                    },
                }, var, other.var);
            }

            constexpr Reference() {}
          public:
            // All references are owned by the list and can't be copied out.
            // That's because we're forced to return true references from dereferencing iterators, so we have to store them in one place.
            // See the iterator class for more details.
            Reference(const Reference &) = delete;
            Reference(Reference &&) = delete;

            // Assignment between references assigns the pointed elements.
            constexpr Reference &operator=(const Reference &other) const noexcept(can_nothrow_copy_assign_elems) requires can_copy_assign_elems
            {
                return std::visit(detail::overload{
                    []<typename A, typename B>(A, B) -> bool requires std::is_same_v<A, std::monostate> || std::is_same_v<A, std::monostate>
                    {
                        BETTER_INIT_ABORT
                    },
                    [](auto a, auto b) -> bool
                    {
                        *a.value = *b.value;
                    },
                }, var, other.var);
            }
            constexpr Reference &operator=(const Reference &&other) const noexcept(can_nothrow_move_assign_elems) requires can_move_assign_elems
            {
                return std::visit(detail::overload{
                    []<typename A, typename B>(A, B) -> bool requires std::is_same_v<A, std::monostate> || std::is_same_v<A, std::monostate>
                    {
                        BETTER_INIT_ABORT
                    },
                    [](auto a, auto b) -> bool
                    {
                        *a.value = std::move(*b.value);
                    },
                }, var, other.var);
            }

            constexpr operator Value() const & noexcept(can_nothrow_copy_extract_elems) requires can_copy_extract_elems
            {
                if (var.index() == 0 || var.valueless_by_exception())
                    BETTER_INIT_ABORT
                Value ret;
                detail::with_constexpr_index<sizeof...(P)>(var.index() - 1, [&](auto index)
                {
                    constexpr std::size_t i = index.value + 1;
                    ret.var.template emplace<i>(static_cast<const std::remove_reference_t<typename std::variant_alternative_t<i, decltype(var)>::tag> &>(std::get<i>(var).value));
                });
                return ret;
            }
            constexpr operator Value() const && noexcept(can_nothrow_copy_extract_elems) requires can_copy_extract_elems
            {
                if (var.index() == 0 || var.valueless_by_exception())
                    BETTER_INIT_ABORT
                Value ret;
                detail::with_constexpr_index<sizeof...(P)>(var.index() - 1, [&](auto index)
                {
                    constexpr std::size_t i = index.value + 1;
                    ret.var.template emplace<i>(static_cast<std::remove_reference_t<typename std::variant_alternative_t<i, decltype(var)>::tag> &&>(std::get<i>(var).value));
                });
                return ret;
            }
            #error need copy and move assignment from value VVVVVV
            constexpr Reference &operator=(const Value &value) const & noexcept(can_nothrow_copy_insert_elems) requires can_copy_insert_elems
            {
                return std::visit(detail::overload{
                    []<typename A, typename B>(A, B) -> bool requires std::is_same_v<A, std::monostate> || std::is_same_v<A, std::monostate>
                    {
                        BETTER_INIT_ABORT
                    },
                    [](auto a, auto b) -> bool
                    {
                        *a.value = *b.value;
                    },
                }, var, other.var);
            }

            template <typename T> requires can_initialize_with_elem<T>
            constexpr operator T() const noexcept(can_nothrow_initialize_with_elem<T>)
            {
                return detail_to<detail::fwd::forward, T>();
            }
            template <typename T> requires can_initialize_with_elem<T &&>
            constexpr operator T &&() const noexcept(can_nothrow_initialize_with_elem<T &&>)
            {
                return detail_to<detail::fwd::forward, T &&>();
            }

            template <typename T> requires can_assign_elem_from<T>
            constexpr const Reference &operator=(T &&other) const noexcept(can_nothrow_assign_elem_from<T>)
            {
                std::visit([&](auto ptr) -> void
                {
                    *ptr.value = std::forward<T>(other);
                }, var);
                return *this;
            }

            // E.g. `std::sort` needs this.
            friend constexpr void swap(const Reference &a, const Reference &b) noexcept(detail::all_nothrow_swappable<P...>) requires detail::all_swappable<P...>
            {
                std::visit([&](auto a, auto b) -> void
                {
                    std::swap(*a.value, *b.value);
                }, a.var, b.var);
            }

            #define DETAIL_BETTER_INIT_CMP(name_, op_, is_eq_) \
                friend constexpr bool operator op_(const Reference &a, const Reference &b) \
                noexcept(detail::cmp::support_nothrow_cmp<detail::cmp::name_, P...>) \
                requires detail::cmp::support_cmp<detail::cmp::name_, P...> \
                {return a.template detail_compare<detail::cmp::name_, is_eq_>(b);}

            DETAIL_BETTER_INIT_CMP_OPS(DETAIL_BETTER_INIT_CMP)
            #undef DETAIL_BETTER_INIT_CMP
        };

        class Iterator
        {
            friend class BETTER_INIT_IDENTIFIER;
            const Reference *ref = nullptr;

          public:
            // C++20 `std::iterator_traits` needs this to auto-detect stuff.
            // Don't want to specify typedefs manually, because I'd need the iterator category tag, and I don't want to include `<iterator>` if I don't have to.
            // Besides, this lets me use `std::iterator_traits` category detection to check iterator correctness.
            using value_type = Value;

            constexpr Iterator() noexcept {}

            // `LegacyForwardIterator` requires us to return an actual reference here.
            constexpr const Reference &operator*() const noexcept {return *ref;}

            // No `operator->`.
            // `std::iterator_traits` uses it to determine the `pointer` type, and this causes it to use `void`. Good enough for us?

            // Don't want to rely on `<compare>`.
            friend constexpr bool operator==(Iterator a, Iterator b) noexcept
            {
                return a.ref == b.ref;
            }
            friend constexpr bool operator!=(Iterator a, Iterator b) noexcept {return !(a == b);}
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
            // There's no `offset - iterator`.

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
        [[no_unique_address]] std::conditional_t<sizeof...(P) == 0, detail::empty, Reference[sizeof...(P)]> elems;

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
