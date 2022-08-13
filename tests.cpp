#ifndef BETTER_INIT_CONFIG // This lets us run tests on godbolt easier, see below.
#include "better_init.hpp"
#endif

// To run on https://gcc.godbolt.org, copy-paste following:
// #include <https://raw.githubusercontent.com/HolyBlackCat/better_init/master/include/better_init.hpp>
// #include <https://raw.githubusercontent.com/HolyBlackCat/better_init/master/include/tests.cpp>
// Or copypaste `better_init.hpp`, followed by this file.

#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <utility>
#include <vector>


// Expands to the preferred init list notation for the current language standard.
#if BETTER_INIT_ALLOW_BRACES
#define INIT(...) init{__VA_ARGS__}
#else
#define INIT(...) init(__VA_ARGS__)
#endif

#ifndef TEST_NON_COPYABLE_TYPES
#if BETTER_INIT_CXX_STANDARD >= 17
#define TEST_NON_COPYABLE_TYPES 1
#else
#define TEST_NON_COPYABLE_TYPES 0
#endif
#endif


#define ASSERT(...) \
    do { \
        if (!bool(__VA_ARGS__)) \
        { \
            std::cout << "Check failed at " __FILE__ ":" << __LINE__ << ": " #__VA_ARGS__ "\n"; \
            better_init::detail::abort(); \
        } \
    } \
    while (false)

#define ASSERT_EQ(a, b) \
    do { \
        if (a != b) \
        { \
            std::cout << "Check failed at " __FILE__ ":" << __LINE__ << ": " #a " == " #b ", expanded to " << a << " == " << b << "\n"; \
            better_init::detail::abort(); \
        } \
    } \
    while (false)


// Detection idiom.
namespace detail_is_detected
{
    namespace impl
    {
        template <typename T, typename ...P> struct dependent_type
        {
            using type = T;
        };
    }
    template <typename A, typename ...B> using void_type = typename impl::dependent_type<void, A, B...>::type;

    template <typename DummyVoid, template <typename...> class A, typename ...B> struct is_detected : std::false_type {};
    template <template <typename...> class A, typename ...B> struct is_detected<void_type<A<B...>>, A, B...> : std::true_type {};
}
template <template <typename...> class A, typename ...B> struct is_detected : detail_is_detected::is_detected<void, A, B...> {};


// Get a `init<P...>` value from element types.
// Causes UB when called, intended only to instantiate templates.
template <typename ...P>
better_init::DETAIL_BETTER_INIT_CLASS_NAME<P...> &&invalid_init_list()
{
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
    static int dummy;
    return (better_init::DETAIL_BETTER_INIT_CLASS_NAME<P...> &&)dummy;
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
}

// A bunch of list variations for us to check.
#define CHECKED_LIST_TYPES(X) \
    /* Target type         | Element types... */\
    X(int,                  ) \
    X(int,                  int, int) \
    X(int,                  int, const int, int &, const int &) \
    X(std::unique_ptr<int>, std::nullptr_t &, std::unique_ptr<int>) \

// Try to explicitly instantiate the types from `CHECKED_LIST_TYPES`.
#define CHECK_INSTANTIATION(target_, ...) \
    template class better_init::DETAIL_BETTER_INIT_CLASS_NAME<__VA_ARGS__>; \
    template class better_init::DETAIL_BETTER_INIT_CLASS_NAME<__VA_ARGS__>::Iterator<target_>; \
    template class better_init::DETAIL_BETTER_INIT_CLASS_NAME<__VA_ARGS__>::Reference<target_>;
CHECKED_LIST_TYPES(CHECK_INSTANTIATION)
#undef CHECK_INSTANTIATION

// Check iterator categories for `CHECKED_LIST_TYPES`.
template <typename T>
struct IteratorCategoryChecker
{
    using value_type = T;

    template <typename U>
    IteratorCategoryChecker(U, U)
    {
        #if BETTER_INIT_CXX_STANDARD >= 20
        static_assert(std::random_access_iterator<U>, "The iterator concept wasn't satisfied.");
        #endif
        static_assert(std::is_same<typename std::iterator_traits<U>::iterator_category, std::random_access_iterator_tag>::value, "Wrong iterator category.");
    }
};
#define CHECK_ITERATOR_CATEGORY(target_, ...) (void)IteratorCategoryChecker<target_>(invalid_init_list<__VA_ARGS__>());
void check_iterator_categories()
{
    if (false) // Just instantiation is enough.
    {
        CHECKED_LIST_TYPES(CHECK_ITERATOR_CATEGORY)
    }
}
#undef CHECK_ITERATOR_CATEGORY

// A fake container, checks iterator sanity.
struct IteratorSanityChecker
{
    using value_type = int;

    template <typename T>
    IteratorSanityChecker(T begin, T end)
    {
        { // Increments and decrements.
            auto iter = begin;
            ASSERT_EQ(int(*iter++), 1);
            iter = begin;
            ASSERT_EQ(int(*++iter), 2);

            iter = begin + 1;
            ASSERT_EQ(int(*iter--), 2);
            iter = begin + 1;
            ASSERT_EQ(int(*--iter), 1);
        }

        { // +, -
            ASSERT_EQ(end - begin, 3);
            ASSERT_EQ(begin - end, -3);
            ASSERT_EQ(begin + 2 - end, -1);
            ASSERT_EQ(2 + begin - end, -1);
            ASSERT_EQ(end - 2 - begin, 1);

            auto iter = begin;
            ASSERT((iter += 2) - end == -1 && iter - end == -1);
            iter = end;
            ASSERT((iter -= 2) - begin == 1 && iter - begin == 1);
        }

        { // Indexing.
            ASSERT_EQ(int(*(begin)), 1);
            ASSERT_EQ(int(*(begin + 0)), 1);
            ASSERT_EQ(int(*(begin + 1)), 2);
            ASSERT_EQ(int(*(begin + 2)), 3);
            ASSERT_EQ(int(*(end - 3)), 1);
            ASSERT_EQ(int(*(end - 2)), 2);
            ASSERT_EQ(int(*(end - 1)), 3);

            ASSERT_EQ(int(begin[0]), 1);
            ASSERT_EQ(int(begin[1]), 2);
            ASSERT_EQ(int(begin[2]), 3);
            ASSERT_EQ(int(end[-3]), 1);
            ASSERT_EQ(int(end[-2]), 2);
            ASSERT_EQ(int(end[-1]), 3);
        }

        { // Comparison operators.
            auto a = begin;
            auto b = begin + 1;
            auto c = begin + 2;

            ASSERT(a != b && b == b && c != b);
            ASSERT(a < b && !(b < a) && !(b < b) && b < c && !(c < b));
            ASSERT(b > a && !(a > b) && !(b > b) && c > b && !(b > c));
            ASSERT(a <= b && !(b <= a) && b <= b && b <= c && !(c <= b));
            ASSERT(b >= a && !(a >= b) && b >= b && c >= b && !(b >= c));
        }
    }
};


// A fake container without a `std::initializer_list` constructor, to check implicit-ness of conversions.
struct ContainerWithoutListCtor
{
    using value_type = int;

    template <typename T>
    ContainerWithoutListCtor(T, T) {}
};

// A fake container that requires extra parameters after two iterators.
struct ContainerWithForcedArgs
{
    using value_type = int;

    template <typename T>
    ContainerWithForcedArgs(T, T, int, int, int) {}
};
template <typename ...P> using MakeContainerWithForcedArgs = decltype(INIT(1, 2, 3).to<ContainerWithForcedArgs>(std::declval<P>()...));


int main()
{
    // Iterator sanity tests.
    (void)IteratorSanityChecker(INIT(1, 2, 3));

    { // Generic usage tests.
        std::vector<std::unique_ptr<int>> vec1 = INIT(nullptr, std::make_unique<int>(42));
        ASSERT_EQ(vec1.size(), 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);

        std::vector<std::unique_ptr<int>> vec2 = INIT(nullptr);
        ASSERT_EQ(vec2.size(), 1);
        ASSERT(vec2[0] == nullptr);

        std::vector<std::unique_ptr<int>> vec3 = INIT();
        ASSERT(vec3.empty());

        #if TEST_NON_COPYABLE_TYPES
        std::vector<std::atomic_int> vec4 = INIT(1, 2, 3);
        ASSERT_EQ(vec4.size(), 3);
        ASSERT_EQ(vec4[0].load(), 1);
        ASSERT_EQ(vec4[1].load(), 2);
        ASSERT_EQ(vec4[2].load(), 3);

        std::vector<std::atomic_int> vec5 = INIT();
        ASSERT(vec5.empty());

        int a = 5;
        const int b = 6;
        std::vector<std::atomic_int> vec6 = INIT(4, a, b);
        ASSERT_EQ(vec6.size(), 3);
        ASSERT_EQ(vec6[0].load(), 4);
        ASSERT_EQ(vec6[1].load(), 5);
        ASSERT_EQ(vec6[2].load(), 6);
        #endif
    }

    { // Explicit initialization.
        // Double parentheses, because GCC 11 becomes confused otherwise. Most probably a bug.
        std::vector<std::unique_ptr<int>> vec1((INIT(nullptr, std::make_unique<int>(42))));
        ASSERT_EQ(vec1.size(), 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);
    }

    { // Brace initialization, as opposed to initialization with pairs iterators.
        std::array<std::unique_ptr<int>, 2> arr1 = INIT(nullptr, std::make_unique<int>(42));
        ASSERT(arr1[0] == nullptr);
        ASSERT(arr1[1] != nullptr && *arr1[1] == 42);

        std::array<std::unique_ptr<int>, 2> arr2 = INIT(std::make_unique<int>(43));
        ASSERT(arr2[0] != nullptr && *arr2[0] == 43);
        ASSERT(arr2[1] == nullptr);

        #if TEST_NON_COPYABLE_TYPES
        std::array<std::atomic_int, 3> arr3 = INIT(1, 2, 3);
        ASSERT_EQ(arr3[0].load(), 1);
        ASSERT_EQ(arr3[1].load(), 2);
        ASSERT_EQ(arr3[2].load(), 3);
        #endif
    }

    { // Maps.
        // From lists of pairs.
        // This needs mandatory copy elision because the target pair element is `const`, see: https://stackoverflow.com/a/73087143
        // GCC and Clang (libstdc++ and libc++) reject this in C++14 and earlier. MSVC (MSVC's library) always accepts. Clang (MSVC's library) wasn't tested.
        #if BETTER_INIT_CXX_STANDARD >= 17
        {
            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map1 = INIT(
                std::make_pair(std::make_unique<int>(1), std::make_unique<float>(2.3f)),
                std::make_pair(std::make_unique<int>(2), std::make_unique<float>(3.4f))
            );
            ASSERT_EQ(map1.size(), 2);
            for (const auto &[key, value] : map1)
            {
                if (*key == 1)
                    ASSERT_EQ(*value, 2.3f);
                else
                    ASSERT_EQ(*value, 3.4f);
            }

            std::map<int, std::atomic_int> map2 = INIT(
                std::make_pair(1, 2),
                std::make_pair(3, 4)
            );
            ASSERT_EQ(map2.size(), 2);
            ASSERT_EQ(map2.at(1).load(), 2);
            ASSERT_EQ(map2.at(3).load(), 4);
        }
        #endif

        #if BETTER_INIT_CXX_STANDARD >= 17
        { // From lists of lists.
            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map3 = INIT(
                INIT(std::make_unique<int>(1), std::make_unique<float>(2.3f)),
                INIT(std::make_unique<int>(2), std::make_unique<float>(3.4f))
            );
            ASSERT_EQ(map3.size(), 2);
            for (const auto &[key, value] : map3)
            {
                if (*key == 1)
                    ASSERT_EQ(*value, 2.3f);
                else
                    ASSERT_EQ(*value, 3.4f);
            }

            std::map<int, std::atomic_int> map4 = INIT(
                INIT(1, 2),
                INIT(3, 4)
            );
            ASSERT_EQ(map4.size(), 2);
            ASSERT_EQ(map4.at(1).load(), 2);
            ASSERT_EQ(map4.at(3).load(), 4);
        }
        #endif
    }

    { // Implicit-ness of the conversion operator.
        static_assert(!std::is_convertible<better_init::DETAIL_BETTER_INIT_CLASS_NAME<int, int>, ContainerWithoutListCtor>::value, "");
        static_assert(std::is_constructible<ContainerWithoutListCtor, better_init::DETAIL_BETTER_INIT_CLASS_NAME<int, int>>::value, "");
        (void)INIT(1, 2).to<ContainerWithoutListCtor>();
    }

    { // Construction with extra arguments.
        static_assert(!std::is_convertible<better_init::DETAIL_BETTER_INIT_CLASS_NAME<int, int>, ContainerWithForcedArgs>::value, "");
        static_assert(!std::is_constructible<ContainerWithForcedArgs, better_init::DETAIL_BETTER_INIT_CLASS_NAME<int, int>>::value, "");
        static_assert(!is_detected<MakeContainerWithForcedArgs>::value, "");
        static_assert(is_detected<MakeContainerWithForcedArgs, int, int, int>::value, "");
        (void)INIT(1, 2, 3).to<ContainerWithForcedArgs>(1, 2, 3);
    }

    std::cout << "OK";
    #if BETTER_INIT_ALLOCATOR_HACK
    std::cout << "  (with allocator hack)";
    if (!better_init::detail::allocator_hack::compiler_has_broken_construct_at::value)
        std::cout << " (the allocator hack is unnecessary)";
    #endif
    std::cout << '\n';
}
