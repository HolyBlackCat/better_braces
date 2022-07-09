#include "better_init.hpp"
// For https://gcc.godbolt.org:
// #include <https://raw.githubusercontent.com/HolyBlackCat/better_init/master/include/better_init.hpp>

#include <atomic>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>


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
namespace detail::is_detected
{
    namespace impl
    {
        template <typename T, typename ...P> struct dependent_type
        {
            using type = T;
        };
    }
    template <typename A, typename ...B> using void_type = typename impl::dependent_type<void, A, B...>::type;

    template <typename DummyVoid, template <typename...> typename A, typename ...B> struct is_detected : std::false_type {};
    template <template <typename...> typename A, typename ...B> struct is_detected<void_type<A<B...>>, A, B...> : std::true_type {};
}
template <template <typename...> typename A, typename ...B> inline constexpr bool is_detected = detail::is_detected::is_detected<void, A, B...>::value;


// Get a `init<P...>` value from element types.
// Causes UB when called, intended only to instantiate templates.
template <typename ...P>
init<P...> &&invalid_init_list()
{
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
    static int dummy;
    return (init<P...> &&)dummy;
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
    template class better_init::init<__VA_ARGS__>; \
    template class better_init::init<__VA_ARGS__>::Iterator<target_>; \
    template class better_init::init<__VA_ARGS__>::Reference<target_>;
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
        static_assert(std::random_access_iterator<U>);
        static_assert(std::is_same_v<typename std::iterator_traits<U>::iterator_category, std::random_access_iterator_tag>);
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
template <typename ...P> using MakeContainerWithForcedArgs = decltype(init{1, 2, 3}.to<ContainerWithForcedArgs>(std::declval<P>()...));


int main()
{
    // Iterator sanity tests.
    (void)IteratorSanityChecker(init{1, 2, 3});

    { // Generic usage tests.
        std::vector<std::unique_ptr<int>> vec1 = init{nullptr, std::make_unique<int>(42)};
        ASSERT(vec1.size() == 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);

        std::vector<std::unique_ptr<int>> vec2 = init{};
        ASSERT(vec2.empty());

        std::vector<std::atomic_int> vec3 = init{1, 2, 3};
        ASSERT(vec3.size() == 3);
        ASSERT(vec3[0].load() == 1);
        ASSERT(vec3[1].load() == 2);
        ASSERT(vec3[2].load() == 3);

        std::vector<std::atomic_int> vec4 = init{};
        ASSERT(vec4.empty());

        int a = 5;
        const int b = 6;
        std::vector<std::atomic_int> vec5 = init{4, a, b};
        ASSERT(vec5.size() == 3);
        ASSERT(vec5[0].load() == 4);
        ASSERT(vec5[1].load() == 5);
        ASSERT(vec5[2].load() == 6);
    }

    { // Implicit-ness of the conversion operator.
        static_assert(!std::is_convertible_v<init<int, int>, ContainerWithoutListCtor>);
        static_assert(std::is_constructible_v<ContainerWithoutListCtor, init<int, int>>);
        (void)init{1, 2}.to<ContainerWithoutListCtor>();
    }

    { // Construction with extra arguments.
        static_assert(!std::is_convertible_v<init<int, int>, ContainerWithForcedArgs>);
        static_assert(!std::is_constructible_v<ContainerWithForcedArgs, init<int, int>>);
        static_assert(!is_detected<MakeContainerWithForcedArgs>);
        static_assert(is_detected<MakeContainerWithForcedArgs, int, int, int>);
        (void)init{1, 2, 3}.to<ContainerWithForcedArgs>(1, 2, 3);
    }

    std::cout << "OK\n";
}
