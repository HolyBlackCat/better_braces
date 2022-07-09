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

#define CHECK_LIST_TYPE(...) \
    template class better_init::init<__VA_ARGS__>; \
    static_assert(std::random_access_iterator<iterator_of<init<__VA_ARGS__>>>); \
    static_assert(std::is_same_v<std::iterator_traits<iterator_of<init<__VA_ARGS__>>>::iterator_category, std::random_access_iterator_tag>);

// Make sure `init<??>` can be instantiated and the iterators have the right category and pass the concept.
CHECK_LIST_TYPE()
CHECK_LIST_TYPE(int, int)
CHECK_LIST_TYPE(int, const int, int &, const int &)
CHECK_LIST_TYPE(std::nullptr_t &, std::unique_ptr<int>)

struct FakeContainerWithoutListCtor
{
    template <typename T>
    FakeContainerWithoutListCtor(T, T) {}
};

int main()
{
    { // Iterator sanity tests.
        int n1 = 1, n2 = 2, n3 = 3;
        init i{n1, n2, n3};
        auto begin = i.begin();
        auto end = i.end();

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
        static_assert(!std::is_convertible_v<init<int, int>, FakeContainerWithoutListCtor>);
        static_assert(std::is_constructible_v<FakeContainerWithoutListCtor, init<int, int>>);
        (void)init{1, 2}.to<FakeContainerWithoutListCtor>();
    }

    std::cout << "OK\n";
}
