
#include <better_init.hpp>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <ranges>

template <typename T>
struct A
{
    void foo()
    {
        T t;
        [[maybe_unused]] auto x = *t;
    }
    static_assert(std::random_access_iterator<T>);
    static_assert(std::is_same_v<typename std::iterator_traits<T>::iterator_category, std::random_access_iterator_tag>);
};
template struct A<better_init::init<int, int>::Iterator>;

static_assert(std::ranges::range<better_init::init<int, int>>);

#include <memory>
#include <vector>

int main()
{
    int x = 1, y = 2;
    std::vector<int> v = init{x, y, 1};

    float f = 1.2;
    float g = 3.3;
    float k = 2.3;
    init i{f, g, k};
    std::sort(i.begin(), i.end());
    std::cout << f << ' ' << g << ' ' << k << '\n';

    std::vector<std::unique_ptr<int>> vec = init{nullptr, std::make_unique<int>(42)};
    for (const auto &elem : vec)
    {
        std::cout << elem.get();
        if (elem)
            std::cout << " -> " << *elem;
        std::cout << '\n';
    }
}

// #include <atomic>
// #include <vector>

// int main()
// {
//     std::vector<std::atomic_int> vec = init{1, 2, 3};
//     for (const auto &elem : vec)
//         std::cout << elem.load() << '\n';
// }
