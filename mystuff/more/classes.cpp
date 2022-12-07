#include <concepts>
#include <iostream>

struct Foo2 {
    size_t len;
    Foo2() { len = 0; }
    size_t getlen() const { return len; }
};

struct Foo3 {
    size_t len;
    Foo3() { len = 0; }
    size_t getlen() const { return len; }
};

template <typename T> size_t getlen(T x) { return x.len; };
template <typename T>
concept IsFoo = requires(T x) {
                    { getlen(x) } -> std::convertible_to<size_t>;
                    { x.getlen() } -> std::convertible_to<size_t>;
                };
template <IsFoo T> void setlen(T& x, size_t len) { x.len = len; }

int main() {
    auto f2 = Foo2();
    auto f3 = Foo3();
    std::cout << f2.len << std::endl;
    setlen(f2, 10);
    std::cout << f2.getlen() << std::endl;
    return 0;
}
