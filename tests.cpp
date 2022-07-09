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
            BETTER_INIT_ABORT \
        } \
    } \
    while (false)

#define ASSERT_EQ(a, b) \
    do { \
        if (a != b) \
        { \
            std::cout << "Check failed at " __FILE__ ":" << __LINE__ << ": " #a " == " #b ", expanded to " << a << " == " << b << "\n"; \
            BETTER_INIT_ABORT \
        } \
    } \
    while (false)

template <typename T> using iterator_of = std::remove_cvref_t<decltype(std::declval<T &>().begin())>;

#define CHECKED_LIST_TYPES(X) \
    /* Target type         | Element types... */\
    X(int,                  ) \
    X(int,                  int, int) \
    X(int,                  int, const int, int &, const int &) \
    X(std::unique_ptr<int>, std::nullptr_t &, std::unique_ptr<int>) \

#define CHECK_LIST_TYPE(target_, ...) \
    template class better_init::init<__VA_ARGS__>; \
    template std::vector<target_> better_init::init<__VA_ARGS__>::to<std::vector<target_>>() const &&;
CHECKED_LIST_TYPES(CHECK_LIST_TYPE)
#undef CHECK_LIST_TYPE

struct ContainerWithoutListCtor
{
    using value_type = int;

    template <typename T>
    ContainerWithoutListCtor(T, T) {}
};

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
template <typename ...Elems, typename Target>
void check_iterator_category_helper(Target *)
{
    if (false)
        (void)IteratorCategoryChecker<Target>(init{(Elems &&)*(std::remove_reference_t<Elems> *)nullptr...});
}
// Pass to `CHECKED_LIST_TYPES` to check iterator categories of our iterators.
#define CHECK_ITERATOR_CATEGORY(target_, ...) check_iterator_category_helper<__VA_ARGS__>((target_ *)nullptr);

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

int main()
{
    // Check iterator categories.
    CHECKED_LIST_TYPES(CHECK_ITERATOR_CATEGORY)

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

    { // Explicit-ness of the conversion operator.
        static_assert(!std::is_convertible_v<init<int, int>, ContainerWithoutListCtor>);
        static_assert(std::is_constructible_v<ContainerWithoutListCtor, init<int, int>>);
        (void)init{1, 2}.to<ContainerWithoutListCtor>();
    }

    std::cout << "OK\n";
}
