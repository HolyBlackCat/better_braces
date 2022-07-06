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
    }

    // Customization points.
    namespace custom
    {
        // Customizes the behavior of `init::to()` and of the implicit conversion to a container.
        template <typename T, typename = void>
        struct range_traits
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
    }

    namespace detail
    {
        template <typename T, typename Iter>
        concept supports_list_init = requires
        {
            custom::range_traits<T>::construct(Iter{}, Iter{});
        };

        template <typename T, typename Iter>
        concept supports_nothrow_list_init = requires
        {
            { custom::range_traits<T>::construct(Iter{}, Iter{}) } noexcept;
        };
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
    template <typename ...P>
    class BETTER_INIT_IDENTIFIER
    {
      public:
        // Whether this list can be used to initialize a range of `T`s.
        template <typename T> static constexpr bool can_initialize_elem         = (std::is_constructible_v        <T, P &&> && ...);
        template <typename T> static constexpr bool can_nothrow_initialize_elem = (std::is_nothrow_constructible_v<T, P &&> && ...);

      private:
        class Reference
        {
            friend BETTER_INIT_IDENTIFIER;
            void *target = nullptr;
            std::size_t index = 0;
          public:
            template <typename T> requires can_initialize_elem<T>
            constexpr operator T() const noexcept(can_nothrow_initialize_elem<T>)
            {
                if constexpr (sizeof...(P) == 0)
                {
                    return *(volatile T *)nullptr;
                }
                else
                {
                    constexpr T (*lambdas[])(void *) = {
                        +[](void *ptr)
                        {
                            // Don't want to include `<utility>` for `std::forward`.
                            return T(static_cast<P &&>(*reinterpret_cast<P *>(ptr)));
                        }...
                    };
                    return lambdas[index](target);
                }
            }
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
        // Those should be `&&`-qualified, but then we no longer satisfy `std::ranges::range`.
        [[nodiscard]] constexpr Iterator begin() noexcept
        {
            Iterator ret;
            ret.ref = elems;
            return ret;
        }
        [[nodiscard]] constexpr Iterator end() noexcept
        {
            Iterator ret;
            ret.ref = elems + sizeof...(P);
            return ret;
        }

        // Convert to a range.
        template <typename T> requires detail::supports_list_init<T, Iterator>
        [[nodiscard]] constexpr T to() && noexcept(detail::supports_nothrow_list_init<T, Iterator>)
        {
            // Don't want to include `<utility>` for `std::move`.
            return custom::range_traits<T>::construct(static_cast<BETTER_INIT_IDENTIFIER &&>(*this).begin(), static_cast<BETTER_INIT_IDENTIFIER &&>(*this).end());
        }

        // Implicitly convert to a range.
        template <typename T> requires detail::supports_list_init<T, Iterator>
        [[nodiscard]] constexpr explicit(!custom::range_traits<T>::implicit) operator T() && noexcept(detail::supports_nothrow_list_init<T, Iterator>)
        {
            // Don't want to include `<utility>` for `std::move`.
            return static_cast<BETTER_INIT_IDENTIFIER &&>(*this).to<T>();
        }
    };

    template <typename ...P>
    BETTER_INIT_IDENTIFIER(P &&...) -> BETTER_INIT_IDENTIFIER<P...>;
}

using better_init::BETTER_INIT_IDENTIFIER;
