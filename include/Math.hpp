#pragma once
// We'll follow Julia style, so anything that's not a constructor, destructor,
// nor an operator will be outside of the struct/class.
#include "./Macro.hpp"
#include "./TypePromotion.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/raw_ostream.h>
#include <optional>
// #include <mlir/Analysis/Presburger/Matrix.h>
#include <numeric>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
// #ifndef NDEBUG
// #include <memory>
// #include <stacktrace>
// using stacktrace =
//     std::basic_stacktrace<std::allocator<std::stacktrace_entry>>;
// #endif

template <typename R>
concept AbstractRange = requires(R r) {
                            { r.begin() };
                            { r.end() };
                        };
llvm::raw_ostream &printRange(llvm::raw_ostream &os, AbstractRange auto &r) {
    os << "[ ";
    bool needComma = false;
    for (auto x : r) {
        if (needComma)
            os << ", ";
        os << x;
        needComma = true;
    }
    os << " ]";
    return os;
}

[[maybe_unused]] static int64_t gcd(int64_t x, int64_t y) {
    if (x == 0) {
        return std::abs(y);
    } else if (y == 0) {
        return std::abs(x);
    }
    assert(x != std::numeric_limits<int64_t>::min());
    assert(y != std::numeric_limits<int64_t>::min());
    int64_t a = std::abs(x);
    int64_t b = std::abs(y);
    if ((a == 1) | (b == 1))
        return 1;
    int64_t az = std::countr_zero(uint64_t(x));
    int64_t bz = std::countr_zero(uint64_t(y));
    b >>= bz;
    int64_t k = std::min(az, bz);
    while (a) {
        a >>= az;
        int64_t d = a - b;
        az = std::countr_zero(uint64_t(d));
        b = std::min(a, b);
        a = std::abs(d);
    }
    return b << k;
}
[[maybe_unused]] static int64_t lcm(int64_t x, int64_t y) {
    if (std::abs(x) == 1)
        return y;
    if (std::abs(y) == 1)
        return x;
    return x * (y / gcd(x, y));
}
// https://en.wikipedia.org/wiki/Extended_Euclidean_algorithm
template <std::integral T> std::tuple<T, T, T> gcdx(T a, T b) {
    T old_r = a;
    T r = b;
    T old_s = 1;
    T s = 0;
    T old_t = 0;
    T t = 1;
    while (r) {
        T quotient = old_r / r;
        old_r -= quotient * r;
        old_s -= quotient * s;
        old_t -= quotient * t;
        std::swap(r, old_r);
        std::swap(s, old_s);
        std::swap(t, old_t);
    }
    // Solving for `t` at the end has 1 extra division, but lets us remove
    // the `t` updates in the loop:
    // T t = (b == 0) ? 0 : ((old_r - old_s * a) / b);
    // For now, I'll favor forgoing the division.
    return std::make_tuple(old_r, old_s, old_t);
}

constexpr std::pair<int64_t, int64_t> divgcd(int64_t x, int64_t y) {
    if (x) {
        if (y) {
            int64_t g = gcd(x, y);
            assert(g == std::gcd(x, y));
            return std::make_pair(x / g, y / g);
        } else {
            return std::make_pair(1, 0);
        }
    } else if (y) {
        return std::make_pair(0, 1);
    } else {
        return std::make_pair(0, 0);
    }
}

// template<typename T> T one(const T) { return T(1); }
struct One {
    operator int64_t() { return 1; };
    operator size_t() { return 1; };
};
bool isOne(int64_t x) { return x == 1; }
bool isOne(size_t x) { return x == 1; }

template <typename TRC> auto powBySquare(TRC &&x, size_t i) {
    // typedef typename std::remove_const<TRC>::type TR;
    // typedef typename std::remove_reference<TR>::type T;
    // typedef typename std::remove_reference<TRC>::type TR;
    // typedef typename std::remove_const<TR>::type T;
    typedef typename std::remove_cvref<TRC>::type T;
    switch (i) {
    case 0:
        return T(One());
    case 1:
        return T(std::forward<TRC>(x));
    case 2:
        return T(x * x);
    case 3:
        return T(x * x * x);
    default:
        break;
    }
    if (isOne(x))
        return T(One());
    int64_t t = std::countr_zero(i) + 1;
    i >>= t;
    // T z(std::move(x));
    T z(std::forward<TRC>(x));
    T b;
    while (--t) {
        b = z;
        z *= b;
    }
    if (i == 0)
        return z;
    T y(z);
    while (i) {
        t = std::countr_zero(i) + 1;
        i >>= t;
        while ((--t) >= 0) {
            b = z;
            z *= b;
        }
        y *= z;
    }
    return y;
}

template <typename T>
concept HasMul = requires(T t) { t.mul(t, t); };

// a and b are temporary, z stores the final results.
template <HasMul T> void powBySquare(T &z, T &a, T &b, T const &x, size_t i) {
    switch (i) {
    case 0:
        z = One();
        return;
    case 1:
        z = x;
        return;
    case 2:
        z.mul(x, x);
        return;
    case 3:
        b.mul(x, x);
        z.mul(b, x);
        return;
    default:
        break;
    }
    if (isOne(x)) {
        z = x;
        return;
    }
    int64_t t = std::countr_zero(i) + 1;
    i >>= t;
    z = x;
    while (--t) {
        b.mul(z, z);
        std::swap(b, z);
    }
    if (i == 0)
        return;
    a = z;
    while (i) {
        t = std::countr_zero(i) + 1;
        i >>= t;
        while ((--t) >= 0) {
            b.mul(a, a);
            std::swap(b, a);
        }
        b.mul(a, z);
        std::swap(b, z);
    }
    return;
}
template <HasMul TRC> auto powBySquare(TRC &&x, size_t i) {
    // typedef typename std::remove_const<TRC>::type TR;
    // typedef typename std::remove_reference<TR>::type T;
    // typedef typename std::remove_reference<TRC>::type TR;
    // typedef typename std::remove_const<TR>::type T;
    typedef typename std::remove_cvref<TRC>::type T;
    switch (i) {
    case 0:
        return T(One());
    case 1:
        return T(std::forward<TRC>(x));
    case 2:
        return T(x * x);
    case 3:
        return T(x * x * x);
    default:
        break;
    }
    if (isOne(x))
        return T(One());
    int64_t t = std::countr_zero(i) + 1;
    i >>= t;
    // T z(std::move(x));
    T z(std::forward<TRC>(x));
    T b;
    while (--t) {
        b.mul(z, z);
        std::swap(b, z);
    }
    if (i == 0)
        return z;
    T y(z);
    while (i) {
        t = std::countr_zero(i) + 1;
        i >>= t;
        while ((--t) >= 0) {
            b.mul(z, z);
            std::swap(b, z);
        }
        b.mul(y, z);
        std::swap(b, y);
    }
    return y;
}

template <typename T, typename S> void divExact(T &x, S const &y) {
    auto d = x / y;
    assert(d * y == x);
    x = d;
}

inline bool isZero(auto x) { return x == 0; }

[[maybe_unused]] static bool allZero(const auto &x) {
    for (auto &a : x)
        if (!isZero(a))
            return false;
    return true;
}
[[maybe_unused]] static bool allGEZero(const auto &x) {
    for (auto &a : x)
        if (a < 0)
            return false;
    return true;
}
[[maybe_unused]] static bool allLEZero(const auto &x) {
    for (auto &a : x)
        if (a > 0)
            return false;
    return true;
}

[[maybe_unused]] static size_t countNonZero(const auto &x) {
    size_t i = 0;
    for (auto &a : x)
        i += (a != 0);
    return i;
}

template <typename T>
concept AbstractVector =
    HasEltype<T> && requires(T t, size_t i) {
                        { t(i) } -> std::convertible_to<eltype_t<T>>;
                        { t.size() } -> std::convertible_to<size_t>;
                        { t.view() };
                        {
                            std::remove_reference_t<T>::canResize
                            } -> std::same_as<const bool &>;
                        // {t.extendOrAssertSize(i)};
                    };
// template <typename T>
// concept AbstractMatrix = HasEltype<T> && requires(T t, size_t i) {
//     { t(i, i) } -> std::convertible_to<typename T::eltype>;
//     { t.numRow() } -> std::convertible_to<size_t>;
//     { t.numCol() } -> std::convertible_to<size_t>;
// };
template <typename T>
concept AbstractMatrixCore =
    HasEltype<T> && requires(T t, size_t i) {
                        { t(i, i) } -> std::convertible_to<eltype_t<T>>;
                        { t.numRow() } -> std::convertible_to<size_t>;
                        { t.numCol() } -> std::convertible_to<size_t>;
                        { t.size() } -> std::same_as<std::pair<size_t, size_t>>;
                        {
                            std::remove_reference_t<T>::canResize
                            } -> std::same_as<const bool &>;
                        // {t.extendOrAssertSize(i, i)};
                    };
template <typename T>
concept AbstractMatrix =
    AbstractMatrixCore<T> && requires(T t, size_t i) {
                                 { t.view() } -> AbstractMatrixCore;
                             };

inline auto &copyto(AbstractVector auto &y, const AbstractVector auto &x) {
    const size_t M = x.size();
    y.extendOrAssertSize(M);
    for (size_t i = 0; i < M; ++i)
        y(i) = x(i);
    return y;
}
inline auto &copyto(AbstractMatrixCore auto &A,
                    const AbstractMatrixCore auto &B) {
    const size_t M = B.numRow();
    const size_t N = B.numCol();
    A.extendOrAssertSize(M, N);
    for (size_t r = 0; r < M; ++r)
        for (size_t c = 0; c < N; ++c)
            A(r, c) = B(r, c);
    return A;
}

bool operator==(const AbstractMatrix auto &A, const AbstractMatrix auto &B) {
    const size_t M = B.numRow();
    const size_t N = B.numCol();
    if ((M != A.numRow()) || (N != A.numCol()))
        return false;
    for (size_t r = 0; r < M; ++r)
        for (size_t c = 0; c < N; ++c)
            if (A(r, c) != B(r, c))
                return false;
    return true;
}

struct Add {
    constexpr auto operator()(auto x, auto y) const { return x + y; }
};
struct Sub {
    constexpr auto operator()(auto x) const { return -x; }
    constexpr auto operator()(auto x, auto y) const { return x - y; }
};
struct Mul {
    constexpr auto operator()(auto x, auto y) const { return x * y; }
};
struct Div {
    constexpr auto operator()(auto x, auto y) const { return x / y; }
};

template <typename Op, typename A> struct ElementwiseUnaryOp {
    using eltype = typename A::eltype;
    [[no_unique_address]] const Op op;
    [[no_unique_address]] const A a;
    static constexpr bool canResize = false;
    auto operator()(size_t i) const { return op(a(i)); }
    auto operator()(size_t i, size_t j) const { return op(a(i, j)); }

    constexpr auto size() const { return a.size(); }
    constexpr size_t numRow() const { return a.numRow(); }
    constexpr size_t numCol() const { return a.numCol(); }
    constexpr auto view() const { return *this; };
};
// scalars broadcast
constexpr auto get(const std::integral auto A, size_t) { return A; }
constexpr auto get(const std::floating_point auto A, size_t) { return A; }
constexpr auto get(const std::integral auto A, size_t, size_t) { return A; }
constexpr auto get(const std::floating_point auto A, size_t, size_t) {
    return A;
}
inline auto get(const AbstractVector auto &A, size_t i) { return A(i); }
inline auto get(const AbstractMatrix auto &A, size_t i, size_t j) {
    return A(i, j);
}

constexpr size_t size(const std::integral auto) { return 1; }
constexpr size_t size(const std::floating_point auto) { return 1; }
constexpr size_t size(const AbstractVector auto &x) { return x.size(); }

struct Rational;
template <typename T>
concept Scalar =
    std::integral<T> || std::floating_point<T> || std::same_as<T, Rational>;

template <typename T>
concept VectorOrScalar = AbstractVector<T> || Scalar<T>;
template <typename T>
concept MatrixOrScalar = AbstractMatrix<T> || Scalar<T>;

template <typename Op, VectorOrScalar A, VectorOrScalar B>
struct ElementwiseVectorBinaryOp {
    using eltype = promote_eltype_t<A, B>;
    [[no_unique_address]] Op op;
    [[no_unique_address]] A a;
    [[no_unique_address]] B b;
    static constexpr bool canResize = false;
    auto operator()(size_t i) const { return op(get(a, i), get(b, i)); }
    constexpr size_t size() const {
        if constexpr (AbstractVector<A> && AbstractVector<B>) {
            const size_t N = a.size();
            assert(N == b.size());
            return N;
        } else if constexpr (AbstractVector<A>) {
            return a.size();
        } else { // if constexpr (AbstractVector<B>) {
            return b.size();
        }
    }
    constexpr auto &view() const { return *this; };
};

template <typename Op, MatrixOrScalar A, MatrixOrScalar B>
struct ElementwiseMatrixBinaryOp {
    using eltype = promote_eltype_t<A, B>;
    [[no_unique_address]] Op op;
    [[no_unique_address]] A a;
    [[no_unique_address]] B b;
    static constexpr bool canResize = false;
    auto operator()(size_t i, size_t j) const {
        return op(get(a, i, j), get(b, i, j));
    }
    constexpr size_t numRow() const {
        static_assert(AbstractMatrix<A> || std::integral<A> ||
                          std::floating_point<A>,
                      "Argument A to elementwise binary op is not a matrix.");
        static_assert(AbstractMatrix<B> || std::integral<B> ||
                          std::floating_point<B>,
                      "Argument B to elementwise binary op is not a matrix.");
        if constexpr (AbstractMatrix<A> && AbstractMatrix<B>) {
            const size_t N = a.numRow();
            assert(N == b.numRow());
            return N;
        } else if constexpr (AbstractMatrix<A>) {
            return a.numRow();
        } else if constexpr (AbstractMatrix<B>) {
            return b.numRow();
        }
    }
    constexpr size_t numCol() const {
        static_assert(AbstractMatrix<A> || std::integral<A> ||
                          std::floating_point<A>,
                      "Argument A to elementwise binary op is not a matrix.");
        static_assert(AbstractMatrix<B> || std::integral<B> ||
                          std::floating_point<B>,
                      "Argument B to elementwise binary op is not a matrix.");
        if constexpr (AbstractMatrix<A> && AbstractMatrix<B>) {
            const size_t N = a.numCol();
            assert(N == b.numCol());
            return N;
        } else if constexpr (AbstractMatrix<A>) {
            return a.numCol();
        } else if constexpr (AbstractMatrix<B>) {
            return b.numCol();
        }
    }
    constexpr std::pair<size_t, size_t> size() const {
        return std::make_pair(numRow(), numCol());
    }
    constexpr auto &view() const { return *this; };
};

template <typename A> struct Transpose {
    using eltype = eltype_t<A>;
    [[no_unique_address]] A a;
    static constexpr bool canResize = false;
    auto operator()(size_t i, size_t j) const { return a(j, i); }
    constexpr size_t numRow() const { return a.numCol(); }
    constexpr size_t numCol() const { return a.numRow(); }
    constexpr auto &view() const { return *this; };
    constexpr std::pair<size_t, size_t> size() const {
        return std::make_pair(numRow(), numCol());
    }
};
template <AbstractMatrix A, AbstractMatrix B> struct MatMatMul {
    using eltype = promote_eltype_t<A, B>;
    [[no_unique_address]] A a;
    [[no_unique_address]] B b;
    static constexpr bool canResize = false;
    auto operator()(size_t i, size_t j) const {
        static_assert(AbstractMatrix<B>, "B should be an AbstractMatrix");
        auto s = (a(i, 0) * b(0, j)) * 0;
        for (size_t k = 0; k < a.numCol(); ++k)
            s += a(i, k) * b(k, j);
        return s;
    }
    constexpr size_t numRow() const { return a.numRow(); }
    constexpr size_t numCol() const { return b.numCol(); }
    constexpr std::pair<size_t, size_t> size() const {
        return std::make_pair(numRow(), numCol());
    }
    constexpr auto view() const { return *this; };
};
template <AbstractMatrix A, AbstractVector B> struct MatVecMul {
    using eltype = promote_eltype_t<A, B>;
    [[no_unique_address]] A a;
    [[no_unique_address]] B b;
    static constexpr bool canResize = false;
    auto operator()(size_t i) const {
        static_assert(AbstractVector<B>, "B should be an AbstractVector");
        auto s = (a(i, 0) * b(0)) * 0;
        for (size_t k = 0; k < a.numCol(); ++k)
            s += a(i, k) * b(k);
        return s;
    }
    constexpr size_t size() const { return a.numRow(); }
    constexpr auto view() const { return *this; };
};

struct Begin {
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Begin) {
        return os << 0;
    }
} begin;
struct End {
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, End) {
        return os << "end";
    }
} end;
struct OffsetBegin {
    [[no_unique_address]] size_t offset;
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, OffsetBegin r) {
        return os << r.offset;
    }
};
constexpr OffsetBegin operator+(size_t x, Begin) { return OffsetBegin{x}; }
constexpr OffsetBegin operator+(Begin, size_t x) { return OffsetBegin{x}; }
constexpr OffsetBegin operator+(size_t x, OffsetBegin y) {
    return OffsetBegin{x + y.offset};
}
inline OffsetBegin operator+(OffsetBegin y, size_t x) {
    return OffsetBegin{x + y.offset};
}
struct OffsetEnd {
    [[no_unique_address]] size_t offset;
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, OffsetEnd r) {
        return os << "end - " << r.offset;
    }
};
constexpr OffsetEnd operator-(End, size_t x) { return OffsetEnd{x}; }
constexpr OffsetEnd operator-(OffsetEnd y, size_t x) {
    return OffsetEnd{y.offset + x};
}
constexpr OffsetEnd operator+(OffsetEnd y, size_t x) {
    return OffsetEnd{y.offset - x};
}

template <typename T>
concept RelativeOffset = std::same_as<T, End> || std::same_as<T, OffsetEnd> ||
                         std::same_as<T, Begin> || std::same_as<T, OffsetBegin>;

template <typename B, typename E> struct Range {
    [[no_unique_address]] B b;
    [[no_unique_address]] E e;
};
template <std::integral B, std::integral E> struct Range<B, E> {
    [[no_unique_address]] B b;
    [[no_unique_address]] E e;
    struct Iterator {
        B i;
        constexpr bool operator==(E e) { return i == e; }
        Iterator &operator++() {
            ++i;
            return *this;
        }
        Iterator operator++(int) {
            Iterator t = *this;
            ++*this;
            return t;
        }
        Iterator &operator--() {
            --i;
            return *this;
        }
        Iterator operator--(int) {
            Iterator t = *this;
            --*this;
            return t;
        }
        B operator*() { return i; }
    };
    constexpr Iterator begin() const { return Iterator{b}; }
    constexpr E end() const { return e; }
    constexpr auto size() const { return e - b; }
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Range<B, E> r) {
        return os << "[" << r.b << ":" << r.e << ")";
    }
};
// template <typename B, typename E>
// constexpr B std::ranges::begin(Range<B,E> r){ return r.b;}

// template <> struct std::iterator_traits<Range<size_t,size_t>> {
//     using difference_type = ptrdiff_t;
//     using iterator_category = std::forward_iterator_tag;
//     using value_type = size_t;
//     using reference_type = void;
//     using pointer_type = void;
// };

// static_assert(std::ranges::range<Range<size_t, size_t>>);

// template <> struct Range<Begin, int> {
//     static constexpr Begin b = begin;
//     int e;
//     operator Range<Begin, size_t>() {
//         return Range<Begin, size_t>{b, size_t(e)};
//     }
// };
// template <> struct Range<int, End> {
//     int b;
//     static constexpr End e = end;
//     operator Range<size_t, End>() { return Range<size_t, End>{size_t(b), e};
//     }
// };
// template <> struct Range<int, int> {
//     int b;
//     int e;
//     operator Range<size_t, size_t>() {
//         return Range<size_t, size_t>{.b = size_t(b), .e = size_t(e)};
//     }
// };
// template <> struct Range<Begin, size_t> {
//     static constexpr Begin b = begin;
//     size_t e;
//     Range(Range<Begin, int> r) : e(r.e){};
// };
// template <> struct Range<size_t, End> {
//     size_t b;
//     static constexpr End e = end;
//     Range(Range<int, End> r) : b(r.b){};
// };
// template <> struct Range<size_t,size_t> {
//     size_t b;
//     size_t e;
//     Range(Range<int, int> r) : b(r.b), e(r.e) {};
// };
struct Colon {
    constexpr Range<size_t, size_t> operator()(std::integral auto i,
                                               std::integral auto j) const {
        return Range<size_t, size_t>{size_t(i), size_t(j)};
    }
    template <RelativeOffset E>
    constexpr Range<size_t, E> operator()(std::integral auto i, E j) const {
        return Range<size_t, E>{size_t(i), j};
    }
    template <RelativeOffset B>
    constexpr Range<B, size_t> operator()(B i, std::integral auto j) const {
        return Range<B, size_t>{i, size_t(j)};
    }
    template <RelativeOffset B, RelativeOffset E>
    constexpr Range<B, E> operator()(B i, E j) const {
        return Range<B, E>{i, j};
    }
} _;

#ifndef NDEBUG
void checkIndex(size_t X, size_t x) { assert(x < X); }
void checkIndex(size_t X, End) { assert(X > 0); }
void checkIndex(size_t X, Begin) { assert(X > 0); }
void checkIndex(size_t X, OffsetEnd x) { assert(x.offset < X); }
void checkIndex(size_t X, OffsetBegin x) { assert(x.offset < X); }
template <typename B> void checkIndex(size_t X, Range<B, size_t> x) {
    assert(x.e <= X);
}
template <typename B, typename E> void checkIndex(size_t, Range<B, E>) {}
void checkIndex(size_t, Colon) {}
#endif

constexpr size_t canonicalize(size_t e, size_t) { return e; }
constexpr size_t canonicalize(Begin, size_t) { return 0; }
constexpr size_t canonicalize(OffsetBegin b, size_t) { return b.offset; }
constexpr size_t canonicalize(End, size_t M) { return M - 1; }
constexpr size_t canonicalize(OffsetEnd e, size_t M) {
    return M - 1 - e.offset;
}

constexpr size_t canonicalizeForRange(size_t e, size_t) { return e; }
constexpr size_t canonicalizeForRange(Begin, size_t) { return 0; }
constexpr size_t canonicalizeForRange(OffsetBegin b, size_t) {
    return b.offset;
}
constexpr size_t canonicalizeForRange(End, size_t M) { return M; }
constexpr size_t canonicalizeForRange(OffsetEnd e, size_t M) {
    return M - e.offset;
}

// Union type
template <typename T>
concept ScalarIndex =
    std::integral<T> || std::same_as<T, End> || std::same_as<T, Begin> ||
    std::same_as<T, OffsetBegin> || std::same_as<T, OffsetEnd>;

template <typename B, typename E>
constexpr Range<size_t, size_t> canonicalizeRange(Range<B, E> r, size_t M) {
    return Range<size_t, size_t>{canonicalizeForRange(r.b, M),
                                 canonicalizeForRange(r.e, M)};
}
constexpr Range<size_t, size_t> canonicalizeRange(Colon, size_t M) {
    return Range<size_t, size_t>{0, M};
}

template <typename B, typename E>
constexpr auto operator+(Range<B, E> r, size_t x) {
    return _(r.b + x, r.e + x);
}
template <typename B, typename E>
constexpr auto operator-(Range<B, E> r, size_t x) {
    return _(r.b - x, r.e - x);
}

template <typename T> struct PtrVector {
    static_assert(!std::is_const_v<T>, "const T is redundant");
    using eltype = T;
    [[no_unique_address]] const T *const mem;
    [[no_unique_address]] const size_t N;
    static constexpr bool canResize = false;
    bool operator==(AbstractVector auto &x) {
        if (N != x.size())
            return false;
        for (size_t n = 0; n < N; ++n)
            if (mem[n] != x(n))
                return false;
        return true;
    }

    const inline T &operator[](const ScalarIndex auto i) const {
#ifndef NDEBUG
        checkIndex(N, i);
#endif
        return mem[canonicalize(i, N)];
    }
    const inline T &operator()(const ScalarIndex auto i) const {
#ifndef NDEBUG
        checkIndex(N, i);
#endif
        return mem[canonicalize(i, N)];
    }
    constexpr PtrVector<T> operator()(Range<size_t, size_t> i) const {
        assert(i.b <= i.e);
        assert(i.e <= N);
        return PtrVector<T>{.mem = mem + i.b, .N = i.e - i.b};
    }
    template <typename F, typename L>
    constexpr PtrVector<T> operator()(Range<F, L> i) const {
        return (*this)(canonicalizeRange(i, N));
    }
    constexpr const T *begin() const { return mem; }
    constexpr const T *end() const { return mem + N; }
    constexpr auto rbegin() const { return std::reverse_iterator(mem + N); }
    constexpr auto rend() const { return std::reverse_iterator(mem); }
    constexpr size_t size() const { return N; }
    constexpr operator llvm::ArrayRef<T>() const {
        return llvm::ArrayRef<T>{mem, N};
    }
    // llvm::ArrayRef<T> arrayref() const { return llvm::ArrayRef<T>(ptr, M); }
    bool operator==(const PtrVector<T> x) const {
        return llvm::ArrayRef<T>(*this) == llvm::ArrayRef<T>(x);
    }
    bool operator==(const llvm::ArrayRef<std::remove_const_t<T>> x) const {
        return llvm::ArrayRef<std::remove_const_t<T>>(*this) == x;
    }
    constexpr PtrVector<T> view() const { return *this; };

    void extendOrAssertSize(size_t M) const { assert(M == N); }
};
template <typename T> struct MutPtrVector {
    static_assert(!std::is_const_v<T>, "T shouldn't be const");
    using eltype = T;
    // using eltype = std::remove_const_t<T>;
    [[no_unique_address]] T *const mem;
    [[no_unique_address]] const size_t N;
    static constexpr bool canResize = false;
    inline T &operator[](const ScalarIndex auto i) {
#ifndef NDEBUG
        checkIndex(N, i);
#endif
        return mem[canonicalize(i, N)];
    }
    inline T &operator()(const ScalarIndex auto i) {
#ifndef NDEBUG
        checkIndex(N, i);
#endif
        return mem[canonicalize(i, N)];
    }
    const inline T &operator[](const ScalarIndex auto i) const {
#ifndef NDEBUG
        checkIndex(N, i);
#endif
        return mem[canonicalize(i, N)];
    }
    const inline T &operator()(const ScalarIndex auto i) const {
#ifndef NDEBUG
        checkIndex(N, i);
#endif
        return mem[canonicalize(i, N)];
    }
    // copy constructor
    // MutPtrVector(const MutPtrVector<T> &x) : mem(x.mem), N(x.N) {}
    constexpr MutPtrVector(const MutPtrVector<T> &x) = default;
    constexpr MutPtrVector(llvm::MutableArrayRef<T> x)
        : mem(x.data()), N(x.size()) {}
    constexpr MutPtrVector(T *mem, size_t N) : mem(mem), N(N) {}
    constexpr MutPtrVector<T> operator()(Range<size_t, size_t> i) {
        assert(i.b <= i.e);
        assert(i.e <= N);
        return MutPtrVector<T>{mem + i.b, i.e - i.b};
    }
    constexpr PtrVector<T> operator()(Range<size_t, size_t> i) const {
        assert(i.b <= i.e);
        assert(i.e <= N);
        return PtrVector<T>{.mem = mem + i.b, .N = i.e - i.b};
    }
    template <typename F, typename L>
    constexpr MutPtrVector<T> operator()(Range<F, L> i) {
        return (*this)(canonicalizeRange(i, N));
    }
    template <typename F, typename L>
    constexpr PtrVector<T> operator()(Range<F, L> i) const {
        return (*this)(canonicalizeRange(i, N));
    }
    constexpr T *begin() { return mem; }
    constexpr T *end() { return mem + N; }
    constexpr const T *begin() const { return mem; }
    constexpr const T *end() const { return mem + N; }
    constexpr size_t size() const { return N; }
    constexpr operator PtrVector<T>() const {
        return PtrVector<T>{.mem = mem, .N = N};
    }
    constexpr operator llvm::ArrayRef<T>() const {
        return llvm::ArrayRef<T>{mem, N};
    }
    constexpr operator llvm::MutableArrayRef<T>() {
        return llvm::MutableArrayRef<T>{mem, N};
    }
    // llvm::ArrayRef<T> arrayref() const { return llvm::ArrayRef<T>(ptr, M); }
    bool operator==(const MutPtrVector<T> x) const {
        return llvm::ArrayRef<T>(*this) == llvm::ArrayRef<T>(x);
    }
    bool operator==(const PtrVector<T> x) const {
        return llvm::ArrayRef<T>(*this) == llvm::ArrayRef<T>(x);
    }
    bool operator==(const llvm::ArrayRef<T> x) const {
        return llvm::ArrayRef<T>(*this) == x;
    }
    constexpr PtrVector<T> view() const { return *this; };
    // PtrVector<T> view() const {
    //     return PtrVector<T>{.mem = mem, .N = N};
    // };
    MutPtrVector<T> operator=(PtrVector<T> x) { return copyto(*this, x); }
    MutPtrVector<T> operator=(MutPtrVector<T> x) { return copyto(*this, x); }
    MutPtrVector<T> operator=(const AbstractVector auto &x) {
        return copyto(*this, x);
    }
    MutPtrVector<T> operator=(std::integral auto x) {
        for (auto &&y : *this)
            y = x;
        return *this;
    }
    MutPtrVector<T> operator+=(const AbstractVector auto &x) {
        assert(N == x.size());
        for (size_t i = 0; i < N; ++i)
            mem[i] += x(i);
        return *this;
    }
    MutPtrVector<T> operator-=(const AbstractVector auto &x) {
        assert(N == x.size());
        for (size_t i = 0; i < N; ++i)
            mem[i] -= x(i);
        return *this;
    }
    MutPtrVector<T> operator*=(const AbstractVector auto &x) {
        assert(N == x.size());
        for (size_t i = 0; i < N; ++i)
            mem[i] *= x(i);
        return *this;
    }
    MutPtrVector<T> operator/=(const AbstractVector auto &x) {
        assert(N == x.size());
        for (size_t i = 0; i < N; ++i)
            mem[i] /= x(i);
        return *this;
    }
    MutPtrVector<T> operator+=(const std::integral auto x) {
        for (size_t i = 0; i < N; ++i)
            mem[i] += x;
        return *this;
    }
    MutPtrVector<T> operator-=(const std::integral auto x) {
        for (size_t i = 0; i < N; ++i)
            mem[i] -= x;
        return *this;
    }
    MutPtrVector<T> operator*=(const std::integral auto x) {
        for (size_t i = 0; i < N; ++i)
            mem[i] *= x;
        return *this;
    }
    MutPtrVector<T> operator/=(const std::integral auto x) {
        for (size_t i = 0; i < N; ++i)
            mem[i] /= x;
        return *this;
    }
    void extendOrAssertSize(size_t M) const { assert(M == N); }
};

//
// Vectors
//

[[maybe_unused]] static int64_t gcd(PtrVector<int64_t> x) {
    int64_t g = std::abs(x[0]);
    for (size_t i = 1; i < x.size(); ++i)
        g = gcd(g, x[i]);
    return g;
}

template <typename T> constexpr auto view(llvm::SmallVectorImpl<T> &x) {
    return MutPtrVector<T>{x.data(), x.size()};
}
template <typename T> constexpr auto view(const llvm::SmallVectorImpl<T> &x) {
    return PtrVector<T>{.mem = x.data(), .N = x.size()};
}
template <typename T> constexpr auto view(llvm::MutableArrayRef<T> x) {
    return MutPtrVector<T>{x.data(), x.size()};
}
template <typename T> constexpr auto view(llvm::ArrayRef<T> x) {
    return PtrVector<T>{.mem = x.data(), .N = x.size()};
}

template <typename T> struct Vector {
    using eltype = T;
    [[no_unique_address]] llvm::SmallVector<T, 16> data;
    static constexpr bool canResize = true;

    Vector(int N) : data(llvm::SmallVector<T>(N)){};
    Vector(size_t N = 0) : data(llvm::SmallVector<T>(N)){};
    Vector(llvm::SmallVector<T> A) : data(std::move(A)){};

    inline T &operator[](const ScalarIndex auto i) {
        return data[canonicalize(i, data.size())];
    }
    inline T &operator()(const ScalarIndex auto i) {
        return data[canonicalize(i, data.size())];
    }
    const inline T &operator[](const ScalarIndex auto i) const {
        return data[canonicalize(i, data.size())];
    }
    const inline T &operator()(const ScalarIndex auto i) const {
        return data[canonicalize(i, data.size())];
    }
    constexpr MutPtrVector<T> operator()(Range<size_t, size_t> i) {
        assert(i.b <= i.e);
        assert(i.e <= data.size());
        return MutPtrVector<T>{data.data() + i.b, i.e - i.b};
    }
    constexpr PtrVector<T> operator()(Range<size_t, size_t> i) const {
        assert(i.b <= i.e);
        assert(i.e <= data.size());
        return PtrVector<T>{.mem = data.data() + i.b, .N = i.e - i.b};
    }
    template <typename F, typename L>
    constexpr MutPtrVector<T> operator()(Range<F, L> i) {
        return (*this)(canonicalizeRange(i, data.size()));
    }
    template <typename F, typename L>
    constexpr PtrVector<T> operator()(Range<F, L> i) const {
        return (*this)(canonicalizeRange(i, data.size()));
    }
    T &operator[](size_t i) { return data[i]; }
    const T &operator[](size_t i) const { return data[i]; }
    // bool operator==(Vector<T, 0> x0) const { return allMatch(*this, x0); }
    constexpr auto begin() { return data.begin(); }
    constexpr auto end() { return data.end(); }
    constexpr auto begin() const { return data.begin(); }
    constexpr auto end() const { return data.end(); }
    constexpr size_t size() const { return data.size(); }
    // MutPtrVector<T> view() {
    //     return MutPtrVector<T>{.mem = data.data(), .N = data.size()};
    // };
    constexpr PtrVector<T> view() const {
        return PtrVector<T>{.mem = data.data(), .N = data.size()};
    };
    template <typename A> void push_back(A &&x) {
        data.push_back(std::forward<A>(x));
    }
    template <typename... A> void emplace_back(A &&...x) {
        data.emplace_back(std::forward<A>(x)...);
    }
    Vector(const AbstractVector auto &x) : data(llvm::SmallVector<T>{}) {
        const size_t N = x.size();
        data.resize_for_overwrite(N);
        for (size_t n = 0; n < N; ++n)
            data[n] = x(n);
    }
    void resize(size_t N) { data.resize(N); }
    void resizeForOverwrite(size_t N) { data.resize_for_overwrite(N); }

    operator MutPtrVector<T>() {
        return MutPtrVector<T>{data.data(), data.size()};
    }
    operator PtrVector<T>() const {
        return PtrVector<T>{.mem = data.data(), .N = data.size()};
    }
    operator llvm::MutableArrayRef<T>() {
        return llvm::MutableArrayRef<T>{data.data(), data.size()};
    }
    operator llvm::ArrayRef<T>() const {
        return llvm::ArrayRef<T>{data.data(), data.size()};
    }
    // MutPtrVector<T> operator=(AbstractVector auto &x) {
    Vector<T> &operator=(const T &x) {
        MutPtrVector<T> y{*this};
        y = x;
        return *this;
    }
    Vector<T> &operator=(AbstractVector auto &x) {
        MutPtrVector<T> y{*this};
        y = x;
        return *this;
    }
    Vector<T> &operator+=(AbstractVector auto &x) {
        MutPtrVector<T> y{*this};
        y += x;
        return *this;
    }
    Vector<T> &operator-=(AbstractVector auto &x) {
        MutPtrVector<T> y{*this};
        y -= x;
        return *this;
    }
    Vector<T> &operator*=(AbstractVector auto &x) {
        MutPtrVector<T> y{*this};
        y *= x;
        return *this;
    }
    Vector<T> &operator/=(AbstractVector auto &x) {
        MutPtrVector<T> y{*this};
        y /= x;
        return *this;
    }
    Vector<T> &operator+=(const std::integral auto x) {
        for (auto &&y : data)
            y += x;
        return *this;
    }
    Vector<T> &operator-=(const std::integral auto x) {
        for (auto &&y : data)
            y -= x;
        return *this;
    }
    Vector<T> &operator*=(const std::integral auto x) {
        for (auto &&y : data)
            y *= x;
        return *this;
    }
    Vector<T> &operator/=(const std::integral auto x) {
        for (auto &&y : data)
            y /= x;
        return *this;
    }
    template <typename... Ts> Vector(Ts... inputs) : data{inputs...} {}
    void clear() { data.clear(); }
    void extendOrAssertSize(size_t N) const { assert(N == data.size()); }
    void extendOrAssertSize(size_t N) {
        if (N != data.size())
            data.resize_for_overwrite(N);
    }
    bool operator==(const Vector<T> &x) const {
        return llvm::ArrayRef<T>(*this) == llvm::ArrayRef<T>(x);
    }
    void pushBack(T x) { data.push_back(std::move(x)); }
};

static_assert(std::copyable<Vector<intptr_t>>);
static_assert(AbstractVector<Vector<int64_t>>);
static_assert(!AbstractVector<int64_t>);

template <typename T> struct StridedVector {
    static_assert(!std::is_const_v<T>, "const T is redundant");
    using eltype = T;
    [[no_unique_address]] const T *const d;
    [[no_unique_address]] const size_t N;
    [[no_unique_address]] const size_t x;
    static constexpr bool canResize = false;
    struct StridedIterator {
        [[no_unique_address]] const T *d;
        [[no_unique_address]] size_t x;
        auto operator++() {
            d += x;
            return *this;
        }
        auto operator--() {
            d -= x;
            return *this;
        }
        const T &operator*() { return *d; }
        bool operator==(const StridedIterator y) const { return d == y.d; }
    };
    constexpr auto begin() const { return StridedIterator{d, x}; }
    constexpr auto end() const { return StridedIterator{d + N * x, x}; }
    const T &operator[](size_t i) const { return d[i * x]; }
    const T &operator()(size_t i) const { return d[i * x]; }

    constexpr StridedVector<T> operator()(Range<size_t, size_t> i) const {
        return StridedVector<T>{.d = d + i.b * x, .N = i.e - i.b, .x = x};
    }
    template <typename F, typename L>
    constexpr StridedVector<T> operator()(Range<F, L> i) const {
        return (*this)(canonicalizeRange(i, N));
    }

    constexpr size_t size() const { return N; }
    bool operator==(StridedVector<T> x) const {
        if (size() != x.size())
            return false;
        for (size_t i = 0; i < size(); ++i) {
            if ((*this)[i] != x[i])
                return false;
        }
        return true;
    }
    constexpr StridedVector<T> view() const { return *this; }
    void extendOrAssertSize(size_t M) const { assert(N == M); }
};
template <typename T> struct MutStridedVector {
    static_assert(!std::is_const_v<T>, "T should not be const");
    using eltype = T;
    [[no_unique_address]] T *const d;
    [[no_unique_address]] const size_t N;
    [[no_unique_address]] const size_t x;
    static constexpr bool canResize = false;
    struct StridedIterator {
        [[no_unique_address]] T *d;
        [[no_unique_address]] size_t x;
        auto operator++() {
            d += x;
            return *this;
        }
        auto operator--() {
            d -= x;
            return *this;
        }
        T &operator*() { return *d; }
        bool operator==(const StridedIterator y) const { return d == y.d; }
    };
    // FIXME: if `x` == 0, then it will not iterate!
    constexpr auto begin() { return StridedIterator{d, x}; }
    constexpr auto end() { return StridedIterator{d + N * x, x}; }
    constexpr auto begin() const { return StridedIterator{d, x}; }
    constexpr auto end() const { return StridedIterator{d + N * x, x}; }
    T &operator[](size_t i) { return d[i * x]; }
    const T &operator[](size_t i) const { return d[i * x]; }
    T &operator()(size_t i) { return d[i * x]; }
    const T &operator()(size_t i) const { return d[i * x]; }

    constexpr MutStridedVector<T> operator()(Range<size_t, size_t> i) {
        return MutStridedVector<T>{.d = d + i.b * x, .N = i.e - i.b, .x = x};
    }
    constexpr StridedVector<T> operator()(Range<size_t, size_t> i) const {
        return StridedVector<T>{.d = d + i.b * x, .N = i.e - i.b, .x = x};
    }
    template <typename F, typename L>
    constexpr MutStridedVector<T> operator()(Range<F, L> i) {
        return (*this)(canonicalizeRange(i, N));
    }
    template <typename F, typename L>
    constexpr StridedVector<T> operator()(Range<F, L> i) const {
        return (*this)(canonicalizeRange(i, N));
    }

    constexpr size_t size() const { return N; }
    // bool operator==(StridedVector<T> x) const {
    //     if (size() != x.size())
    //         return false;
    //     for (size_t i = 0; i < size(); ++i) {
    //         if ((*this)[i] != x[i])
    //             return false;
    //     }
    //     return true;
    // }
    constexpr operator StridedVector<T>() {
        const T *const p = d;
        return StridedVector<T>{.d = p, .N = N, .x = x};
    }
    constexpr StridedVector<T> view() const {
        return StridedVector<T>{.d = d, .N = N, .x = x};
    }
    MutStridedVector<T> &operator=(const T &y) {
        for (size_t i = 0; i < N; ++i)
            d[i * x] = y;
        return *this;
    }
    MutStridedVector<T> &operator=(const AbstractVector auto &x) {
        return copyto(*this, x);
    }
    MutStridedVector<T> &operator=(const MutStridedVector<T> &x) {
        return copyto(*this, x);
    }
    MutStridedVector<T> &operator+=(const AbstractVector auto &x) {
        const size_t M = x.size();
        MutStridedVector<T> &self = *this;
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            self(i) += x(i);
        return self;
    }
    MutStridedVector<T> &operator-=(const AbstractVector auto &x) {
        const size_t M = x.size();
        MutStridedVector<T> &self = *this;
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            self(i) -= x(i);
        return self;
    }
    MutStridedVector<T> &operator*=(const AbstractVector auto &x) {
        const size_t M = x.size();
        MutStridedVector<T> &self = *this;
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            self(i) *= x(i);
        return self;
    }
    MutStridedVector<T> &operator/=(const AbstractVector auto &x) {
        const size_t M = x.size();
        MutStridedVector<T> &self = *this;
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            self(i) /= x(i);
        return self;
    }
    void extendOrAssertSize(size_t M) const { assert(N == M); }
};

template <typename T>
concept DerivedMatrix =
    requires(T t, const T ct) {
        {
            t.data()
            } -> std::convertible_to<typename std::add_pointer_t<
                typename std::add_const_t<typename T::eltype>>>;
        {
            ct.data()
            } -> std::same_as<typename std::add_pointer_t<
                typename std::add_const_t<typename T::eltype>>>;
        { t.numRow() } -> std::convertible_to<size_t>;
        { t.numCol() } -> std::convertible_to<size_t>;
        { t.rowStride() } -> std::convertible_to<size_t>;
    };

template <typename T> struct PtrMatrix;
template <typename T> struct MutPtrMatrix;

template <typename T>
[[maybe_unused]] static inline T &matrixGet(T *ptr, size_t M, size_t N,
                                            size_t X, const ScalarIndex auto m,
                                            const ScalarIndex auto n) {
#ifndef NDEBUG
    checkIndex(M, m);
    checkIndex(N, n);
#endif
    return *(ptr + (canonicalize(n, N) + canonicalize(m, M) * X));
}
template <typename T>
[[maybe_unused]] static inline const T &
matrixGet(const T *ptr, size_t M, size_t N, size_t X, const ScalarIndex auto m,
          const ScalarIndex auto n) {
#ifndef NDEBUG
    checkIndex(M, m);
    checkIndex(N, n);
#endif
    return *(ptr + (canonicalize(n, N) + canonicalize(m, M) * X));
}

template <typename T>
concept AbstractSlice = requires(T t, size_t M) {
                            {
                                canonicalizeRange(t, M)
                                } -> std::same_as<Range<size_t, size_t>>;
                        };

template <typename T>
[[maybe_unused]] static inline constexpr PtrMatrix<T>
matrixGet(const T *ptr, size_t M, size_t N, size_t X,
          const AbstractSlice auto m, const AbstractSlice auto n) {
#ifndef NDEBUG
    checkIndex(M, m);
    checkIndex(N, n);
#endif
    Range<size_t, size_t> mr = canonicalizeRange(m, M);
    Range<size_t, size_t> nr = canonicalizeRange(n, N);
    return PtrMatrix<T>{ptr + nr.b + mr.b * X, mr.e - mr.b, nr.e - nr.b, X};
}
template <typename T>
[[maybe_unused]] static inline constexpr MutPtrMatrix<T>
matrixGet(T *ptr, size_t M, size_t N, size_t X, const AbstractSlice auto m,
          const AbstractSlice auto n) {
#ifndef NDEBUG
    checkIndex(M, m);
    checkIndex(N, n);
#endif
    Range<size_t, size_t> mr = canonicalizeRange(m, M);
    Range<size_t, size_t> nr = canonicalizeRange(n, N);
    return MutPtrMatrix<T>{ptr + nr.b + mr.b * X, mr.e - mr.b, nr.e - nr.b, X};
}

template <typename T>
[[maybe_unused]] static inline constexpr PtrVector<T>
matrixGet(const T *ptr, size_t M, size_t N, size_t X, const ScalarIndex auto m,
          const AbstractSlice auto n) {
#ifndef NDEBUG
    checkIndex(M, m);
    checkIndex(N, n);
#endif
    size_t mi = canonicalize(m, M);
    Range<size_t, size_t> nr = canonicalizeRange(n, N);
    return PtrVector<T>{ptr + nr.b + mi * X, nr.e - nr.b};
}
template <typename T>
[[maybe_unused]] static inline constexpr MutPtrVector<T>
matrixGet(T *ptr, size_t M, size_t N, size_t X, const ScalarIndex auto m,
          const AbstractSlice auto n) {
#ifndef NDEBUG
    checkIndex(M, m);
    checkIndex(N, n);
#endif
    size_t mi = canonicalize(m, M);
    Range<size_t, size_t> nr = canonicalizeRange(n, N);
    return MutPtrVector<T>{ptr + nr.b + mi * X, nr.e - nr.b};
}

template <typename T>
[[maybe_unused]] static inline constexpr StridedVector<T>
matrixGet(const T *ptr, size_t M, size_t N, size_t X,
          const AbstractSlice auto m, const ScalarIndex auto n) {
#ifndef NDEBUG
    checkIndex(M, m);
    checkIndex(N, n);
#endif
    Range<size_t, size_t> mr = canonicalizeRange(m, M);
    size_t ni = canonicalize(n, N);
    return StridedVector<T>{ptr + ni + mr.b * X, mr.e - mr.b, X};
}
template <typename T>
[[maybe_unused]] static inline constexpr MutStridedVector<T>
matrixGet(T *ptr, size_t M, size_t N, size_t X, const AbstractSlice auto m,
          const ScalarIndex auto n) {
#ifndef NDEBUG
    checkIndex(M, m);
    checkIndex(N, n);
#endif
    Range<size_t, size_t> mr = canonicalizeRange(m, M);
    size_t ni = canonicalize(n, N);
    return MutStridedVector<T>{ptr + ni + mr.b * X, mr.e - mr.b, X};
}

constexpr bool isSquare(const AbstractMatrix auto &A) {
    return A.numRow() == A.numCol();
}

template <typename T> constexpr MutStridedVector<T> diag(MutPtrMatrix<T> A) {
    return MutStridedVector<T>{A.data(), std::min(A.numRow(), A.numCol()),
                               A.rowStride() + 1};
}
template <typename T> constexpr StridedVector<T> diag(PtrMatrix<T> A) {
    return StridedVector<T>{A.data(), std::min(A.numRow(), A.numCol()),
                            A.rowStride() + 1};
}
template <typename T>
constexpr MutStridedVector<T> antiDiag(MutPtrMatrix<T> A) {
    return MutStridedVector<T>{A.data() + A.numCol() - 1,
                               std::min(A.numRow(), A.numCol()),
                               A.rowStride() - 1};
}
template <typename T> constexpr StridedVector<T> antiDiag(PtrMatrix<T> A) {
    return StridedVector<T>{A.data() + A.numCol() - 1,
                            std::min(A.numRow(), A.numCol()),
                            A.rowStride() - 1};
}

#define DEFINEMATRIXMEMBERCONST                                                \
    inline const T &operator()(const ScalarIndex auto m,                       \
                               const ScalarIndex auto n) const {               \
        return matrixGet(data(), numRow(), numCol(), rowStride(), m, n);       \
    }                                                                          \
    constexpr auto operator()(auto m, auto n) const {                          \
        return matrixGet(data(), numRow(), numCol(), rowStride(), m, n);       \
    }                                                                          \
    constexpr std::pair<size_t, size_t> size() const {                         \
        return std::make_pair(numRow(), numCol());                             \
    }                                                                          \
    constexpr auto diag() const { return ::diag(PtrMatrix<eltype>(*this)); }   \
    constexpr auto antiDiag() const {                                          \
        return ::antiDiag(PtrMatrix<eltype>(*this));                           \
    }
#define DEFINEMATRIXMEMBERMUT                                                  \
    inline T &operator()(const ScalarIndex auto m, const ScalarIndex auto n) { \
        return matrixGet(data(), numRow(), numCol(), rowStride(), m, n);       \
    }                                                                          \
    constexpr auto operator()(auto m, auto n) {                                \
        return matrixGet(data(), numRow(), numCol(), rowStride(), m, n);       \
    }                                                                          \
    constexpr auto diag() { return ::diag(MutPtrMatrix<eltype>(*this)); }      \
    constexpr auto antiDiag() {                                                \
        return ::antiDiag(MutPtrMatrix<eltype>(*this));                        \
    }

#define DEFINEPTRMATRIXCVT                                                     \
    constexpr operator MutPtrMatrix<T>() {                                     \
        return MutPtrMatrix<T>{data(), numRow(), numCol(), rowStride()};       \
    }                                                                          \
    constexpr operator PtrMatrix<T>() const {                                  \
        return PtrMatrix<T>{                                                   \
            .mem = data(), .M = numRow(), .N = numCol(), .X = rowStride()};    \
    }                                                                          \
    constexpr MutPtrMatrix<T> view() {                                         \
        return MutPtrMatrix<T>{data(), numRow(), numCol(), rowStride()};       \
    }                                                                          \
    constexpr PtrMatrix<T> view() const {                                      \
        return PtrMatrix<T>{                                                   \
            .mem = data(), .M = numRow(), .N = numCol(), .X = rowStride()};    \
    }                                                                          \
    constexpr bool isSquare() const { return numRow() == numCol(); }           \
    constexpr Transpose<PtrMatrix<T>> transpose() const {                      \
        return Transpose<PtrMatrix<T>>{view()};                                \
    }

template <typename T> struct SmallSparseMatrix;
template <typename T> struct PtrMatrix {
    using eltype = std::remove_reference_t<T>;
    static_assert(!std::is_const_v<T>, "const T is redundant");
    static constexpr bool canResize = false;
    [[no_unique_address]] const T *const mem;
    [[no_unique_address]] const size_t M, N, X;

    constexpr const T *data() const { return mem; }
    constexpr size_t numRow() const { return M; }
    constexpr size_t numCol() const { return N; }
    constexpr size_t rowStride() const { return X; }

    DEFINEMATRIXMEMBERCONST

    constexpr bool isSquare() const { return M == N; }
    // Vector<T> diag() const {
    //     size_t K = std::min(M, N);
    //     Vector<T> d;
    //     d.resizeForOverwrite(K);
    //     for (size_t k = 0; k < K; ++k)
    //         d(k) = mem[k * (1 + X)];
    //     return d;
    // }
    constexpr inline PtrMatrix<T> view() const { return *this; };
    constexpr Transpose<PtrMatrix<T>> transpose() const {
        return Transpose<PtrMatrix<T>>{*this};
    }
    void extendOrAssertSize(size_t MM, size_t NN) const {
        assert(MM == M);
        assert(NN == N);
    }
};
template <typename T> struct MutPtrMatrix {
    using eltype = std::remove_reference_t<T>;
    static_assert(!std::is_const_v<T>,
                  "MutPtrMatrix should never have const T");
    [[no_unique_address]] T *const mem;
    [[no_unique_address]] const size_t M, N, X;
    static constexpr bool canResize = false;

    static constexpr bool fixedNumRow = true;
    static constexpr bool fixedNumCol = true;
    constexpr size_t numRow() const { return M; }
    constexpr size_t numCol() const { return N; }
    constexpr size_t rowStride() const { return X; }
    constexpr T *data() { return mem; }
    constexpr const T *data() const { return mem; }
    constexpr PtrMatrix<T> view() const {
        return PtrMatrix<T>{.mem = data(), .M = M, .N = N, .X = X};
    };
    DEFINEMATRIXMEMBERCONST
    DEFINEMATRIXMEMBERMUT
    constexpr operator PtrMatrix<T>() const {
        return PtrMatrix<T>{
            .mem = data(), .M = numRow(), .N = numCol(), .X = rowStride()};
    }

    MutPtrMatrix<T> operator=(const SmallSparseMatrix<T> &A) {
        assert(M == A.numRow());
        assert(N == A.numCol());
        size_t k = 0;
        for (size_t i = 0; i < M; ++i) {
            uint32_t m = A.rows[i] & 0x00ffffff;
            size_t j = 0;
            while (m) {
                uint32_t tz = std::countr_zero(m);
                m >>= tz + 1;
                j += tz;
                mem[i * X + (j++)] = A.nonZeros[k++];
            }
        }
        assert(k == A.nonZeros.size());
        return *this;
    }
    MutPtrMatrix<T> operator=(MutPtrMatrix<T> A) {
        return copyto(*this, PtrMatrix<T>(A));
    }
    // rule of 5 requires...
    constexpr MutPtrMatrix(const MutPtrMatrix<T> &A) = default;
    constexpr MutPtrMatrix(T *mem, size_t M, size_t N)
        : mem(mem), M(M), N(N), X(N){};
    constexpr MutPtrMatrix(T *mem, size_t M, size_t N, size_t X)
        : mem(mem), M(M), N(N), X(X){};

    MutPtrMatrix<T> operator=(const AbstractMatrix auto &B) {
        return copyto(*this, B);
    }
    MutPtrMatrix<T> operator=(const std::integral auto b) {
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) = b;
        return *this;
    }
    MutPtrMatrix<T> operator+=(const AbstractMatrix auto &B) {
        assert(M == B.numRow());
        assert(N == B.numCol());
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) += B(r, c);
        return *this;
    }
    MutPtrMatrix<T> operator-=(const AbstractMatrix auto &B) {
        assert(M == B.numRow());
        assert(N == B.numCol());
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) -= B(r, c);
        return *this;
    }
    MutPtrMatrix<T> operator*=(const std::integral auto b) {
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) *= b;
        return *this;
    }
    MutPtrMatrix<T> operator/=(const std::integral auto b) {
        const size_t M = numRow();
        const size_t N = numCol();
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) /= b;
        return *this;
    }
    constexpr bool isSquare() const { return M == N; }
    // Vector<T> diag() const {
    //     size_t K = std::min(M, N);
    //     Vector<T> d;
    //     d.resizeForOverwrite(N);
    //     for (size_t k = 0; k < K; ++k)
    //         d(k) = mem[k * (1 + X)];
    //     return d;
    // }
    constexpr Transpose<PtrMatrix<T>> transpose() const {
        return Transpose<PtrMatrix<T>>{view()};
    }
    void extendOrAssertSize(size_t M, size_t N) const {
        assert(numRow() == M);
        assert(numCol() == N);
    }
};
template <typename T> constexpr auto ptrVector(T *p, size_t M) {
    if constexpr (std::is_const_v<T>) {
        return PtrVector<std::remove_const_t<T>>{.mem = p, .N = M};
    } else {
        return MutPtrVector<T>{p, M};
    }
}

// template <typename T>
// constexpr auto ptrmat(T *ptr, size_t numRow, size_t numCol, size_t stride) {
//     if constexpr (std::is_const_v<T>) {
//         return PtrMatrix<std::remove_const_t<T>>{
//             .mem = ptr, .M = numRow, .N = numCol, .X = stride};
//     } else {
//         return MutPtrMatrix<T>{
//             .mem = ptr, .M = numRow, .N = numCol, .X = stride};
//     }
// }

static_assert(std::is_trivially_copyable_v<PtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> is not trivially copyable!");
static_assert(std::is_trivially_copyable_v<PtrVector<int64_t>>,
              "PtrVector<int64_t,0> is not trivially copyable!");

static_assert(!AbstractVector<PtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractVector succeeded");
static_assert(!AbstractVector<MutPtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractVector succeeded");
static_assert(!AbstractVector<const PtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractVector succeeded");

static_assert(AbstractMatrix<PtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractMatrix failed");
static_assert(AbstractMatrix<MutPtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractMatrix failed");
static_assert(AbstractMatrix<const PtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractMatrix failed");
static_assert(AbstractMatrix<const MutPtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractMatrix failed");

static_assert(AbstractVector<MutPtrVector<int64_t>>,
              "PtrVector<int64_t> isa AbstractVector failed");
static_assert(AbstractVector<PtrVector<int64_t>>,
              "PtrVector<const int64_t> isa AbstractVector failed");
static_assert(AbstractVector<const PtrVector<int64_t>>,
              "PtrVector<const int64_t> isa AbstractVector failed");
static_assert(AbstractVector<const MutPtrVector<int64_t>>,
              "PtrVector<const int64_t> isa AbstractVector failed");

static_assert(AbstractVector<Vector<int64_t>>,
              "PtrVector<int64_t> isa AbstractVector failed");

static_assert(!AbstractMatrix<MutPtrVector<int64_t>>,
              "PtrVector<int64_t> isa AbstractMatrix succeeded");
static_assert(!AbstractMatrix<PtrVector<int64_t>>,
              "PtrVector<const int64_t> isa AbstractMatrix succeeded");
static_assert(!AbstractMatrix<const PtrVector<int64_t>>,
              "PtrVector<const int64_t> isa AbstractMatrix succeeded");
static_assert(!AbstractMatrix<const MutPtrVector<int64_t>>,
              "PtrVector<const int64_t> isa AbstractMatrix succeeded");

static_assert(
    AbstractMatrix<ElementwiseMatrixBinaryOp<Mul, PtrMatrix<int64_t>, int>>,
    "ElementwiseBinaryOp isa AbstractMatrix failed");

static_assert(
    !AbstractVector<MatMatMul<PtrMatrix<int64_t>, PtrMatrix<int64_t>>>,
    "MatMul should not be an AbstractVector!");
static_assert(AbstractMatrix<MatMatMul<PtrMatrix<int64_t>, PtrMatrix<int64_t>>>,
              "MatMul is not an AbstractMatrix!");

template <typename T>
concept IntVector = requires(T t, int64_t y) {
                        { t.size() } -> std::convertible_to<size_t>;
                        { t[y] } -> std::convertible_to<int64_t>;
                    };

//
// Matrix
//
template <typename T, size_t M = 0, size_t N = 0, size_t S = 64> struct Matrix {
    // using eltype = std::remove_cv_t<T>;
    using eltype = std::remove_reference_t<T>;
    // static_assert(M * N == S,
    //               "if specifying non-zero M and N, we should have M*N == S");
    static constexpr bool fixedNumRow = M;
    static constexpr bool fixedNumCol = N;
    static constexpr bool canResize = false;
    static constexpr bool isMutable = true;
    T mem[S];
    static constexpr size_t numRow() { return M; }
    static constexpr size_t numCol() { return N; }
    static constexpr size_t rowStride() { return N; }

    constexpr T *data() { return mem; }
    constexpr const T *data() const { return mem; }

    DEFINEMATRIXMEMBERCONST
    DEFINEMATRIXMEMBERMUT
    DEFINEPTRMATRIXCVT
    static constexpr size_t getConstCol() { return N; }
};

template <typename T, size_t M, size_t S> struct Matrix<T, M, 0, S> {
    using eltype = std::remove_reference_t<T>;
    [[no_unique_address]] llvm::SmallVector<T, S> mem;
    [[no_unique_address]] size_t N, X;
    static constexpr bool canResize = true;
    static constexpr bool isMutable = true;

    Matrix(size_t n) : mem(llvm::SmallVector<T, S>(M * n)), N(n), X(n){};

    constexpr size_t numRow() const { return M; }
    constexpr size_t numCol() const { return N; }
    constexpr size_t rowStride() const { return X; }

    constexpr T *data() { return mem.data(); }
    constexpr const T *data() const { return mem.data(); }
    DEFINEMATRIXMEMBERCONST
    DEFINEMATRIXMEMBERMUT
    DEFINEPTRMATRIXCVT
    void resizeColsForOverwrite(size_t NN, size_t XX) {
        N = NN;
        X = XX;
        mem.resize_for_overwrite(M * XX);
    }
    void resizeColsForOverwrite(size_t NN) { resizeColsForOverwrite(NN, NN); }
};
template <typename T, size_t N, size_t S> struct Matrix<T, 0, N, S> {
    using eltype = std::remove_reference_t<T>;
    [[no_unique_address]] llvm::SmallVector<T, S> mem;
    [[no_unique_address]] size_t M;
    static constexpr bool canResize = true;
    static constexpr bool isMutable = true;

    Matrix(size_t m) : mem(llvm::SmallVector<T, S>(m * N)), M(m){};

    constexpr inline size_t numRow() const { return M; }
    static constexpr size_t numCol() { return N; }
    static constexpr size_t rowStride() { return N; }
    static constexpr size_t getConstCol() { return N; }

    constexpr T *data() { return mem.data(); }
    constexpr const T *data() const { return mem.data(); }
    DEFINEMATRIXMEMBERCONST
    DEFINEMATRIXMEMBERMUT
    DEFINEPTRMATRIXCVT
};

template <typename T> struct SquarePtrMatrix {
    using eltype = std::remove_reference_t<T>;
    static_assert(!std::is_const_v<T>, "const T is redundant");
    [[no_unique_address]] const T *const mem;
    [[no_unique_address]] const size_t M;
    static constexpr bool fixedNumCol = true;
    static constexpr bool fixedNumRow = true;
    static constexpr bool canResize = false;
    static constexpr bool isMutable = false;

    constexpr size_t numRow() const { return M; }
    constexpr size_t numCol() const { return M; }
    constexpr size_t rowStride() const { return M; }
    constexpr const T *data() { return mem; }
    constexpr const T *data() const { return mem; }
    DEFINEMATRIXMEMBERCONST
    DEFINEMATRIXMEMBERMUT
    DEFINEPTRMATRIXCVT
};
template <typename T> struct MutSquarePtrMatrix {
    using eltype = std::remove_reference_t<T>;
    static_assert(!std::is_const_v<T>, "T should not be const");
    [[no_unique_address]] T *const mem;
    [[no_unique_address]] const size_t M;
    static constexpr bool fixedNumCol = true;
    static constexpr bool fixedNumRow = true;
    static constexpr bool canResize = false;
    static constexpr bool isMutable = true;

    constexpr size_t numRow() const { return M; }
    constexpr size_t numCol() const { return M; }
    constexpr size_t rowStride() const { return M; }

    constexpr T *data() { return mem; }
    constexpr const T *data() const { return mem; }
    constexpr operator SquarePtrMatrix<T>() const {
        return SquarePtrMatrix<T>{mem, M};
    }
    MutSquarePtrMatrix<T> operator=(const AbstractMatrix auto &B) {
        return copyto(*this, B);
    }
    DEFINEMATRIXMEMBERCONST
    DEFINEMATRIXMEMBERMUT
    DEFINEPTRMATRIXCVT
};

template <typename T, unsigned STORAGE = 8> struct SquareMatrix {
    using eltype = std::remove_reference_t<T>;
    static constexpr unsigned TOTALSTORAGE = STORAGE * STORAGE;
    [[no_unique_address]] llvm::SmallVector<T, TOTALSTORAGE> mem;
    [[no_unique_address]] size_t M;
    static constexpr bool fixedNumCol = true;
    static constexpr bool fixedNumRow = true;
    static constexpr bool canResize = false;
    static constexpr bool isMutable = true;

    SquareMatrix(size_t m)
        : mem(llvm::SmallVector<T, TOTALSTORAGE>(m * m)), M(m){};

    constexpr size_t numRow() const { return M; }
    constexpr size_t numCol() const { return M; }
    constexpr size_t rowStride() const { return M; }

    constexpr T *data() { return mem.data(); }
    constexpr const T *data() const { return mem.data(); }

    constexpr T *begin() { return data(); }
    constexpr T *end() { return data() + M * M; }
    constexpr const T *begin() const { return data(); }
    constexpr const T *end() const { return data() + M * M; }
    T &operator[](size_t i) { return mem[i]; }
    const T &operator[](size_t i) const { return mem[i]; }

    static SquareMatrix<T> identity(size_t N) {
        SquareMatrix<T> A(N);
        for (size_t r = 0; r < N; ++r)
            A(r, r) = 1;
        return A;
    }
    constexpr operator MutSquarePtrMatrix<T>() {
        return MutSquarePtrMatrix<T>{mem.data(), size_t(M)};
    }
    constexpr operator SquarePtrMatrix<T>() const {
        return SquarePtrMatrix<T>{mem.data(), M};
    }
    DEFINEMATRIXMEMBERCONST
    DEFINEMATRIXMEMBERMUT
    DEFINEPTRMATRIXCVT
};

template <typename T, size_t S> struct Matrix<T, 0, 0, S> {
    using eltype = std::remove_reference_t<T>;
    [[no_unique_address]] llvm::SmallVector<T, S> mem;

    [[no_unique_address]] size_t M, N, X;
    static constexpr bool canResize = true;
    static constexpr bool isMutable = true;

    constexpr T *data() { return mem.data(); }
    constexpr const T *data() const { return mem.data(); }
    DEFINEPTRMATRIXCVT
    DEFINEMATRIXMEMBERCONST
    DEFINEMATRIXMEMBERMUT
    Matrix(llvm::SmallVector<T, S> content, size_t m, size_t n)
        : mem(std::move(content)), M(m), N(n), X(n){};

    Matrix(size_t m, size_t n)
        : mem(llvm::SmallVector<T, S>(m * n)), M(m), N(n), X(n){};

    Matrix() : M(0), N(0), X(0){};
    Matrix(SquareMatrix<T> &&A)
        : mem(std::move(A.mem)), M(A.M), N(A.M), X(A.M){};
    Matrix(const SquareMatrix<T> &A)
        : mem(A.begin(), A.end()), M(A.M), N(A.M), X(A.M){};
    Matrix(const AbstractMatrix auto &A)
        : mem(llvm::SmallVector<T>{}), M(A.numRow()), N(A.numCol()),
          X(A.numCol()) {
        mem.resize_for_overwrite(M * N);
        for (size_t m = 0; m < M; ++m)
            for (size_t n = 0; n < N; ++n)
                mem[m * X + n] = A(m, n);
    }
    constexpr auto begin() { return mem.begin(); }
    constexpr auto end() { return mem.begin() + rowStride() * M; }
    constexpr auto begin() const { return mem.begin(); }
    constexpr auto end() const { return mem.begin() + rowStride() * M; }
    constexpr size_t numRow() const { return M; }
    constexpr size_t numCol() const { return N; }
    constexpr size_t rowStride() const { return X; }

    static Matrix<T, 0, 0, S> uninitialized(size_t MM, size_t NN) {
        Matrix<T, 0, 0, S> A(0, 0);
        A.M = MM;
        A.X = A.N = NN;
        A.mem.resize_for_overwrite(MM * NN);
        return A;
    }
    static Matrix<T, 0, 0, S> identity(size_t MM) {
        Matrix<T, 0, 0, S> A(MM, MM);
        for (size_t i = 0; i < MM; ++i) {
            A(i, i) = 1;
        }
        return A;
    }
    void clear() {
        M = N = X = 0;
        mem.clear();
    }

    void resize(size_t MM, size_t NN, size_t XX) {
        mem.resize(MM * XX);
        size_t minMMM = std::min(M, MM);
        if ((XX > X) && M && N)
            // need to copy
            for (size_t m = minMMM - 1; m > 0; --m)
                for (size_t n = N; n-- > 0;)
                    mem[m * XX + n] = mem[m * X + n];
        // zero
        for (size_t m = 0; m < minMMM; ++m)
            for (size_t n = N; n < NN; ++n)
                mem[m * XX + n] = 0;
        for (size_t m = minMMM; m < MM; ++m)
            for (size_t n = 0; n < NN; ++n)
                mem[m * XX + n] = 0;
        X = XX;
        M = MM;
        N = NN;
    }
    void insertZeroColumn(size_t i) {
        llvm::errs() << "before";
        CSHOWLN(*this);
        size_t NN = N + 1;
        size_t XX = std::max(X, NN);
        mem.resize(M * XX);
        size_t nLower = (XX > X) ? 0 : i;
        if (M && N)
            // need to copy
            for (size_t m = M; m-- > 0;)
                for (size_t n = N; n-- > nLower;)
                    mem[m * XX + n + (n >= i)] = mem[m * X + n];
        // zero
        for (size_t m = 0; m < M; ++m)
            mem[m * XX + i] = 0;
        X = XX;
        N = NN;
        llvm::errs() << "after";
        CSHOWLN(*this);
    }
    void resize(size_t MM, size_t NN) { resize(MM, NN, std::max(NN, X)); }
    void reserve(size_t MM, size_t NN) { mem.reserve(MM * std::max(X, NN)); }
    void resizeForOverwrite(size_t MM, size_t NN, size_t XX) {
        assert(XX >= NN);
        M = MM;
        N = NN;
        X = XX;
        if (M * X > mem.size())
            mem.resize_for_overwrite(M * X);
    }
    void resizeForOverwrite(size_t MM, size_t NN) {
        M = MM;
        X = N = NN;
        if (M * X > mem.size())
            mem.resize_for_overwrite(M * X);
    }

    void resizeRows(size_t MM) {
        size_t Mold = M;
        M = MM;
        if (M * rowStride() > mem.size())
            mem.resize(M * X);
        if (M > Mold)
            (*this)(_(Mold, M), _) = 0;
    }
    void resizeRowsForOverwrite(size_t MM) {
        if (MM * rowStride() > mem.size())
            mem.resize_for_overwrite(M * X);
        M = MM;
    }
    void resizeCols(size_t NN) { resize(M, NN); }
    void resizeColsForOverwrite(size_t NN) {
        if (NN > X) {
            X = NN;
            mem.resize_for_overwrite(M * X);
        }
        N = NN;
    }
    void eraseCol(size_t i) {
        assert(i < N);
        // TODO: optimize this to reduce copying
        for (size_t m = 0; m < M; ++m)
            for (size_t n = 0; n < N; ++n)
                mem.erase(mem.begin() + m * X + n);
        --N;
        --X;
    }
    void eraseRow(size_t i) {
        assert(i < M);
        auto it = mem.begin() + i * X;
        mem.erase(it, it + X);
        --M;
    }
    void truncateCols(size_t NN) {
        assert(NN <= N);
        N = NN;
    }
    void truncateRows(size_t MM) {
        assert(MM <= M);
        M = MM;
    }
    Matrix<T, 0, 0, S> &operator=(T x) {
        const size_t M = numRow();
        const size_t N = numCol();
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) = x;
        return *this;
    }
    void moveColLast(size_t j) {
        if (j == N)
            return;
        for (size_t m = 0; m < M; ++m) {
            auto x = (*this)(m, j);
            for (size_t n = j; n < N - 1;) {
                size_t o = n++;
                (*this)(m, o) = (*this)(m, n);
            }
            (*this)(m, N - 1) = x;
        }
    }
    Matrix<T, 0, 0, S> deleteCol(size_t c) const {
        Matrix<T, 0, 0, S> A(M, N - 1);
        for (size_t m = 0; m < M; ++m) {
            A(m, _(0, c)) = (*this)(m, _(0, c));
            A(m, _(c, ::end)) = (*this)(m, _(c + 1, ::end));
        }
        return A;
    }
};
typedef Matrix<int64_t> IntMatrix;
static_assert(AbstractMatrix<IntMatrix>);
static_assert(AbstractMatrix<Transpose<IntMatrix>>);

llvm::raw_ostream &printVectorImpl(llvm::raw_ostream &os,
                                   const AbstractVector auto &a) {
    os << "[ ";
    if (size_t M = a.size()) {
        os << a[0];
        for (size_t m = 1; m < M; m++) {
            os << ", " << a[m];
        }
    }
    os << " ]";
    return os;
}
template <typename T>
llvm::raw_ostream &printVector(llvm::raw_ostream &os, PtrVector<T> a) {
    return printVectorImpl(os, a);
}
template <typename T>
llvm::raw_ostream &printVector(llvm::raw_ostream &os, StridedVector<T> a) {
    return printVectorImpl(os, a);
}
template <typename T>
llvm::raw_ostream &printVector(llvm::raw_ostream &os,
                               const llvm::SmallVectorImpl<T> &a) {
    return printVector(os, PtrVector<T>{a.data(), a.size()});
}

template <typename T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, PtrVector<T> const &A) {
    return printVector(os, A);
}
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const AbstractVector auto &A) {
    return printVector(os, A.view());
}

bool allMatch(const AbstractVector auto &x0, const AbstractVector auto &x1) {
    size_t N = x0.size();
    if (N != x1.size())
        return false;
    for (size_t n = 0; n < N; ++n)
        if (x0(n) != x1(n))
            return false;
    return true;
}

MULTIVERSION inline void swapRows(MutPtrMatrix<int64_t> A, size_t i, size_t j) {
    if (i == j)
        return;
    const size_t N = A.numCol();
    assert((i < A.numRow()) && (j < A.numRow()));
    VECTORIZE
    for (size_t n = 0; n < N; ++n)
        std::swap(A(i, n), A(j, n));
}
MULTIVERSION inline void swapCols(MutPtrMatrix<int64_t> A, size_t i, size_t j) {
    if (i == j) {
        return;
    }
    const size_t M = A.numRow();
    assert((i < A.numCol()) && (j < A.numCol()));
    VECTORIZE
    for (size_t m = 0; m < M; ++m)
        std::swap(A(m, i), A(m, j));
}
template <typename T>
[[maybe_unused]] static void swapCols(llvm::SmallVectorImpl<T> &A, size_t i,
                                      size_t j) {
    std::swap(A[i], A[j]);
}
template <typename T>
[[maybe_unused]] static void swapRows(llvm::SmallVectorImpl<T> &A, size_t i,
                                      size_t j) {
    std::swap(A[i], A[j]);
}

template <int Bits, class T>
constexpr bool is_uint_v =
    sizeof(T) == (Bits / 8) && std::is_integral_v<T> && !std::is_signed_v<T>;

template <class T>
constexpr T zeroUpper(T x)
requires is_uint_v<16, T>
{
    return x & 0x00ff;
}
template <class T>
constexpr T zeroLower(T x)
requires is_uint_v<16, T>
{
    return x & 0xff00;
}
template <class T>
constexpr T upperHalf(T x)
requires is_uint_v<16, T>
{
    return x >> 8;
}

template <class T>
constexpr T zeroUpper(T x)
requires is_uint_v<32, T>
{
    return x & 0x0000ffff;
}
template <class T>
constexpr T zeroLower(T x)
requires is_uint_v<32, T>
{
    return x & 0xffff0000;
}
template <class T>
constexpr T upperHalf(T x)
requires is_uint_v<32, T>
{
    return x >> 16;
}
template <class T>
constexpr T zeroUpper(T x)
requires is_uint_v<64, T>
{
    return x & 0x00000000ffffffff;
}
template <class T>
constexpr T zeroLower(T x)
requires is_uint_v<64, T>
{
    return x & 0xffffffff00000000;
}
template <class T>
constexpr T upperHalf(T x)
requires is_uint_v<64, T>
{
    return x >> 32;
}

template <typename T>
[[maybe_unused]] static std::pair<size_t, T> findMax(llvm::ArrayRef<T> x) {
    size_t i = 0;
    T max = std::numeric_limits<T>::min();
    for (size_t j = 0; j < x.size(); ++j) {
        T xj = x[j];
        if (max < xj) {
            max = xj;
            i = j;
        }
    }
    return std::make_pair(i, max);
}

template <class T, int Bits>
concept is_int_v = std::signed_integral<T> && sizeof(T) == (Bits / 8);

template <is_int_v<64> T> constexpr __int128_t widen(T x) { return x; }
template <is_int_v<32> T> constexpr int64_t splitInt(T x) { return x; }

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template <typename T>
concept TriviallyCopyableVectorOrScalar =
    std::is_trivially_copyable_v<T> && VectorOrScalar<T>;
template <typename T>
concept TriviallyCopyableMatrixOrScalar =
    std::is_trivially_copyable_v<T> && MatrixOrScalar<T>;

static_assert(std::copy_constructible<PtrMatrix<int64_t>>);
// static_assert(std::is_trivially_copyable_v<MutPtrMatrix<int64_t>>);
static_assert(std::is_trivially_copyable_v<PtrMatrix<int64_t>>);
static_assert(TriviallyCopyableMatrixOrScalar<PtrMatrix<int64_t>>);
static_assert(TriviallyCopyableMatrixOrScalar<int>);
static_assert(TriviallyCopyable<Mul>);
static_assert(TriviallyCopyableMatrixOrScalar<
              ElementwiseMatrixBinaryOp<Mul, PtrMatrix<int64_t>, int>>);
static_assert(TriviallyCopyableMatrixOrScalar<
              MatMatMul<PtrMatrix<int64_t>, PtrMatrix<int64_t>>>);

template <TriviallyCopyable OP, TriviallyCopyableVectorOrScalar A,
          TriviallyCopyableVectorOrScalar B>
constexpr auto _binaryOp(OP op, A a, B b) {
    return ElementwiseVectorBinaryOp<OP, A, B>{.op = op, .a = a, .b = b};
}
template <TriviallyCopyable OP, TriviallyCopyableMatrixOrScalar A,
          TriviallyCopyableMatrixOrScalar B>
constexpr auto _binaryOp(OP op, A a, B b) {
    return ElementwiseMatrixBinaryOp<OP, A, B>{.op = op, .a = a, .b = b};
}

// template <TriviallyCopyable OP, TriviallyCopyable A, TriviallyCopyable B>
// inline auto binaryOp(const OP op, const A a, const B b) {
//     return _binaryOp(op, a, b);
// }
// template <TriviallyCopyable OP, typename A, TriviallyCopyable B>
// inline auto binaryOp(const OP op, const A &a, const B b) {
//     return _binaryOp(op, a.view(), b);
// }
// template <TriviallyCopyable OP, TriviallyCopyable A, typename B>
// inline auto binaryOp(const OP op, const A a, const B &b) {
//     return _binaryOp(op, a, b.view());
// }
template <TriviallyCopyable OP, typename A, typename B>
constexpr auto binaryOp(const OP op, const A &a, const B &b) {
    if constexpr (std::is_trivially_copyable_v<A>) {
        if constexpr (std::is_trivially_copyable_v<B>) {
            return _binaryOp(op, a, b);
        } else {
            return _binaryOp(op, a, b.view());
        }
    } else if constexpr (std::is_trivially_copyable_v<B>) {
        return _binaryOp(op, a.view(), b);
    } else {
        return _binaryOp(op, a.view(), b.view());
    }
}

constexpr auto bin2(std::integral auto x) { return (x * (x - 1)) >> 1; }

struct Rational {
    [[no_unique_address]] int64_t numerator{0};
    [[no_unique_address]] int64_t denominator{1};

    constexpr Rational() : numerator(0), denominator(1){};
    constexpr Rational(int64_t coef) : numerator(coef), denominator(1){};
    constexpr Rational(int coef) : numerator(coef), denominator(1){};
    constexpr Rational(int64_t n, int64_t d)
        : numerator(d > 0 ? n : -n), denominator(n ? (d > 0 ? d : -d) : 1) {}
    constexpr static Rational create(int64_t n, int64_t d) {
        if (n) {
            int64_t sign = 2 * (d > 0) - 1;
            int64_t g = gcd(n, d);
            n *= sign;
            d *= sign;
            if (g != 1) {
                n /= g;
                d /= g;
            }
            return Rational{n, d};
        } else {
            return Rational{0, 1};
        }
    }
    constexpr static Rational createPositiveDenominator(int64_t n, int64_t d) {
        if (n) {
            int64_t g = gcd(n, d);
            if (g != 1) {
                n /= g;
                d /= g;
            }
            return Rational{n, d};
        } else {
            return Rational{0, 1};
        }
    }

    constexpr std::optional<Rational> safeAdd(Rational y) const {
        auto [xd, yd] = divgcd(denominator, y.denominator);
        int64_t a, b, n, d;
        bool o1 = __builtin_mul_overflow(numerator, yd, &a);
        bool o2 = __builtin_mul_overflow(y.numerator, xd, &b);
        bool o3 = __builtin_mul_overflow(denominator, yd, &d);
        bool o4 = __builtin_add_overflow(a, b, &n);
        if ((o1 | o2) | (o3 | o4)) {
            return {};
        } else if (n) {
            auto [nn, nd] = divgcd(n, d);
            return Rational{nn, nd};
        } else {
            return Rational{0, 1};
        }
    }
    constexpr Rational operator+(Rational y) const { return *safeAdd(y); }
    constexpr Rational &operator+=(Rational y) {
        std::optional<Rational> a = *this + y;
        assert(a.has_value());
        *this = *a;
        return *this;
    }
    constexpr std::optional<Rational> safeSub(Rational y) const {
        auto [xd, yd] = divgcd(denominator, y.denominator);
        int64_t a, b, n, d;
        bool o1 = __builtin_mul_overflow(numerator, yd, &a);
        bool o2 = __builtin_mul_overflow(y.numerator, xd, &b);
        bool o3 = __builtin_mul_overflow(denominator, yd, &d);
        bool o4 = __builtin_sub_overflow(a, b, &n);
        if ((o1 | o2) | (o3 | o4)) {
            return std::optional<Rational>();
        } else if (n) {
            auto [nn, nd] = divgcd(n, d);
            return Rational{nn, nd};
        } else {
            return Rational{0, 1};
        }
    }
    constexpr Rational operator-(Rational y) const {
        return *safeSub(y);
    }
    constexpr Rational &operator-=(Rational y) {
        std::optional<Rational> a = *this - y;
        assert(a.has_value());
        *this = *a;
        return *this;
    }
    constexpr std::optional<Rational> safeMul(int64_t y) const {
        auto [xd, yn] = divgcd(denominator, y);
        int64_t n;
        if (__builtin_mul_overflow(numerator, yn, &n)) {
            return std::optional<Rational>();
        } else {
            return Rational{n, xd};
        }
    }
    constexpr std::optional<Rational> safeMul(Rational y) const {
        if ((numerator != 0) & (y.numerator != 0)) {
            auto [xn, yd] = divgcd(numerator, y.denominator);
            auto [xd, yn] = divgcd(denominator, y.numerator);
            int64_t n, d;
            bool o1 = __builtin_mul_overflow(xn, yn, &n);
            bool o2 = __builtin_mul_overflow(xd, yd, &d);
            if (o1 | o2) {
                return std::optional<Rational>();
            } else {
                return Rational{n, d};
            }
        } else {
            return Rational{0, 1};
        }
    }
    constexpr Rational operator*(int64_t y) const {
        return *safeMul(y);
    }
    constexpr Rational operator*(Rational y) const {
        return *safeMul(y);
    }
    constexpr Rational &operator*=(Rational y) {
        if ((numerator != 0) & (y.numerator != 0)) {
            auto [xn, yd] = divgcd(numerator, y.denominator);
            auto [xd, yn] = divgcd(denominator, y.numerator);
            numerator = xn * yn;
            denominator = xd * yd;
        } else {
            numerator = 0;
            denominator = 1;
        }
        return *this;
    }
    constexpr Rational inv() const {
        if (numerator < 0) {
            // make sure we don't have overflow
            assert(denominator != std::numeric_limits<int64_t>::min());
            return Rational{-denominator, -numerator};
        } else {
            return Rational{denominator, numerator};
        }
        // return Rational{denominator, numerator};
        // bool positive = numerator > 0;
        // return Rational{positive ? denominator : -denominator,
        //                 positive ? numerator : -numerator};
    }
    constexpr std::optional<Rational> safeDiv(Rational y) const {
        return (*this) * y.inv();
    }
    constexpr Rational operator/(Rational y) const {
        return *safeDiv(y);
    }
    // *this -= a*b
    constexpr bool fnmadd(Rational a, Rational b) {
        if (std::optional<Rational> ab = a.safeMul(b)) {
            if (std::optional<Rational> c = safeSub(*ab)) {
                *this = *c;
                return false;
            }
        }
        return true;
    }
    constexpr bool div(Rational a) {
        if (std::optional<Rational> d = safeDiv(a)) {
            *this = *d;
            return false;
        }
        return true;
    }
    // Rational operator/=(Rational y) { return (*this) *= y.inv(); }
    constexpr operator double() { return numerator / denominator; }

    constexpr bool operator==(Rational y) const {
        return (numerator == y.numerator) & (denominator == y.denominator);
    }
    constexpr bool operator!=(Rational y) const {
        return (numerator != y.numerator) | (denominator != y.denominator);
    }
    constexpr bool isEqual(int64_t y) const {
        if (denominator == 1)
            return (numerator == y);
        else if (denominator == -1)
            return (numerator == -y);
        else
            return false;
    }
    constexpr bool operator==(int y) const { return isEqual(y); }
    constexpr bool operator==(int64_t y) const { return isEqual(y); }
    constexpr bool operator!=(int y) const { return !isEqual(y); }
    constexpr bool operator!=(int64_t y) const { return !isEqual(y); }
    constexpr bool operator<(Rational y) const {
        return (widen(numerator) * widen(y.denominator)) <
               (widen(y.numerator) * widen(denominator));
    }
    constexpr bool operator<=(Rational y) const {
        return (widen(numerator) * widen(y.denominator)) <=
               (widen(y.numerator) * widen(denominator));
    }
    constexpr bool operator>(Rational y) const {
        return (widen(numerator) * widen(y.denominator)) >
               (widen(y.numerator) * widen(denominator));
    }
    constexpr bool operator>=(Rational y) const {
        return (widen(numerator) * widen(y.denominator)) >=
               (widen(y.numerator) * widen(denominator));
    }
    constexpr bool operator>=(int y) const { return *this >= Rational(y); }

    friend constexpr bool isZero(Rational x) { return x.numerator == 0; }
    friend constexpr bool isOne(Rational x) {
        return (x.numerator == x.denominator);
    }
    constexpr bool isInteger() const { return denominator == 1; }
    constexpr void negate() { numerator = -numerator; }
    constexpr operator bool() const { return numerator != 0; }

    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                         const Rational &x) {
        os << x.numerator;
        if (x.denominator != 1) {
            os << " // " << x.denominator;
        }
        return os;
    }
    void dump() const { llvm::errs() << *this << "\n"; }

    template <AbstractMatrix B> constexpr auto operator+(B &&b) {
        return binaryOp(Add{}, *this, std::forward<B>(b));
    }
    template <AbstractVector B> constexpr auto operator+(B &&b) {
        return binaryOp(Add{}, *this, std::forward<B>(b));
    }
    template <AbstractMatrix B> constexpr auto operator-(B &&b) {
        return binaryOp(Sub{}, *this, std::forward<B>(b));
    }
    template <AbstractVector B> constexpr auto operator-(B &&b) {
        return binaryOp(Sub{}, *this, std::forward<B>(b));
    }
    template <AbstractMatrix B> constexpr auto operator/(B &&b) {
        return binaryOp(Div{}, *this, std::forward<B>(b));
    }
    template <AbstractVector B> constexpr auto operator/(B &&b) {
        return binaryOp(Div{}, *this, std::forward<B>(b));
    }

    template <AbstractVector B> constexpr auto operator*(B &&b) {
        return binaryOp(Mul{}, *this, std::forward<B>(b));
    }
    template <AbstractMatrix B> constexpr auto operator*(B &&b) {
        return binaryOp(Mul{}, *this, std::forward<B>(b));
    }
};
std::optional<Rational> gcd(Rational x, Rational y) {
    return Rational{gcd(x.numerator, y.numerator),
                    lcm(x.denominator, y.denominator)};
}
int64_t denomLCM(PtrVector<Rational> x) {
    int64_t l = 1;
    for (auto r : x)
        l = lcm(l, r.denominator);
    return l;
}

template <> struct GetEltype<Rational> {
    using eltype = Rational;
};
template <> struct PromoteType<Rational, Rational> {
    using eltype = Rational;
};
template <std::integral I> struct PromoteType<I, Rational> {
    using eltype = Rational;
};
template <std::integral I> struct PromoteType<Rational, I> {
    using eltype = Rational;
};

[[maybe_unused]] static void normalizeByGCD(MutPtrVector<int64_t> x) {
    if (size_t N = x.size()) {
        if (N == 1) {
            x[0] = 1;
            return;
        }
        int64_t g = gcd(x[0], x[1]);
        for (size_t n = 2; (n < N) & (g != 1); ++n)
            g = gcd(g, x[n]);
        if (g > 1)
            for (auto &&a : x)
                a /= g;
    }
}

template <typename T>
llvm::raw_ostream &printMatrix(llvm::raw_ostream &os, PtrMatrix<T> A) {
    // llvm::raw_ostream &printMatrix(llvm::raw_ostream &os, T const &A) {
    auto [m, n] = A.size();
    if (m == 0)
        return os << "[ ]";
    for (size_t i = 0; i < m; i++) {
        if (i) {
            os << "  ";
        } else {
            os << "\n[ ";
        }
        for (int64_t j = 0; j < int64_t(n) - 1; j++) {
            auto Aij = A(i, j);
            if (Aij >= 0) {
                os << " ";
            }
            os << Aij << " ";
        }
        if (n) {
            auto Aij = A(i, n - 1);
            if (Aij >= 0) {
                os << " ";
            }
            os << Aij;
        }
        if (i != m - 1) {
            os << "\n";
        }
    }
    os << " ]";
    return os;
}

template <typename T> struct SmallSparseMatrix {
    // non-zeros
    [[no_unique_address]] llvm::SmallVector<T> nonZeros;
    // masks, the upper 8 bits give the number of elements in previous rows
    // the remaining 24 bits are a mask indicating non-zeros within this row
    static constexpr size_t maxElemPerRow = 24;
    [[no_unique_address]] llvm::SmallVector<uint32_t> rows;
    [[no_unique_address]] size_t col;
    static constexpr bool canResize = false;
    constexpr size_t numRow() const { return rows.size(); }
    constexpr size_t numCol() const { return col; }
    SmallSparseMatrix(size_t numRows, size_t numCols)
        : nonZeros{}, rows{llvm::SmallVector<uint32_t>(numRows)}, col{numCols} {
        assert(col <= maxElemPerRow);
    }
    T get(size_t i, size_t j) const {
        assert(j < col);
        uint32_t r(rows[i]);
        uint32_t jshift = uint32_t(1) << j;
        if (r & (jshift)) {
            // offset from previous rows
            uint32_t prevRowOffset = r >> maxElemPerRow;
            uint32_t rowOffset = std::popcount(r & (jshift - 1));
            return nonZeros[rowOffset + prevRowOffset];
        } else {
            return 0;
        }
    }
    constexpr T operator()(size_t i, size_t j) const { return get(i, j); }
    void insert(T x, size_t i, size_t j) {
        assert(j < col);
        uint32_t r{rows[i]};
        uint32_t jshift = uint32_t(1) << j;
        // offset from previous rows
        uint32_t prevRowOffset = r >> maxElemPerRow;
        uint32_t rowOffset = std::popcount(r & (jshift - 1));
        size_t k = rowOffset + prevRowOffset;
        if (r & jshift) {
            nonZeros[k] = std::move(x);
        } else {
            nonZeros.insert(nonZeros.begin() + k, std::move(x));
            rows[i] = r | jshift;
            for (size_t k = i + 1; k < rows.size(); ++k)
                rows[k] += uint32_t(1) << maxElemPerRow;
        }
    }

    struct Reference {
        [[no_unique_address]] SmallSparseMatrix<T> *A;
        [[no_unique_address]] size_t i, j;
        operator T() const { return A->get(i, j); }
        void operator=(T x) {
            A->insert(std::move(x), i, j);
            return;
        }
    };
    Reference operator()(size_t i, size_t j) { return Reference{this, i, j}; }
    operator Matrix<T>() {
        Matrix<T> A(numRow(), numCol());
        assert(numRow() == A.numRow());
        assert(numCol() == A.numCol());
        size_t k = 0;
        for (size_t i = 0; i < numRow(); ++i) {
            uint32_t m = rows[i] & 0x00ffffff;
            size_t j = 0;
            while (m) {
                uint32_t tz = std::countr_zero(m);
                m >>= tz + 1;
                j += tz;
                A(i, j++) = nonZeros[k++];
            }
        }
        assert(k == nonZeros.size());
        return A;
    }
};

template <typename T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                              SmallSparseMatrix<T> const &A) {
    size_t k = 0;
    os << "[ ";
    for (size_t i = 0; i < A.numRow(); ++i) {
        if (i)
            os << "  ";
        uint32_t m = A.rows[i] & 0x00ffffff;
        size_t j = 0;
        while (m) {
            if (j)
                os << " ";
            uint32_t tz = std::countr_zero(m);
            m >>= (tz + 1);
            j += (tz + 1);
            while (tz--)
                os << " 0 ";
            const T &x = A.nonZeros[k++];
            if (x >= 0)
                os << " ";
            os << x;
        }
        for (; j < A.numCol(); ++j)
            os << "  0";
        os << "\n";
    }
    os << " ]";
    assert(k == A.nonZeros.size());
    return os;
}
template <typename T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, PtrMatrix<T> A) {
    return printMatrix(os, A);
}
template <AbstractMatrix T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const T &A) {
    Matrix<std::remove_const_t<typename T::eltype>> B{A};
    return printMatrix(os, PtrMatrix<typename T::eltype>(B));
}

constexpr auto operator-(const AbstractVector auto &a) {
    auto AA{a.view()};
    return ElementwiseUnaryOp<Sub, decltype(AA)>{.op = Sub{}, .a = AA};
}
constexpr auto operator-(const AbstractMatrix auto &a) {
    auto AA{a.view()};
    return ElementwiseUnaryOp<Sub, decltype(AA)>{.op = Sub{}, .a = AA};
}
static_assert(AbstractMatrix<ElementwiseUnaryOp<Sub, PtrMatrix<int64_t>>>);
static_assert(AbstractMatrix<SquareMatrix<int64_t>>);

template <AbstractMatrix A, typename B> constexpr auto operator+(A &&a, B &&b) {
    return binaryOp(Add{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractVector A, typename B> constexpr auto operator+(A &&a, B &&b) {
    return binaryOp(Add{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractMatrix B>
constexpr auto operator+(std::integral auto a, B &&b) {
    return binaryOp(Add{}, a, std::forward<B>(b));
}
template <AbstractVector B>
constexpr auto operator+(std::integral auto a, B &&b) {
    return binaryOp(Add{}, a, std::forward<B>(b));
}

template <AbstractMatrix A, typename B> constexpr auto operator-(A &&a, B &&b) {
    return binaryOp(Sub{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractVector A, typename B> constexpr auto operator-(A &&a, B &&b) {
    return binaryOp(Sub{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractMatrix B>
constexpr auto operator-(std::integral auto a, B &&b) {
    return binaryOp(Sub{}, a, std::forward<B>(b));
}
template <AbstractVector B>
constexpr auto operator-(std::integral auto a, B &&b) {
    return binaryOp(Sub{}, a, std::forward<B>(b));
}

template <AbstractMatrix A, typename B> constexpr auto operator/(A &&a, B &&b) {
    return binaryOp(Div{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractVector A, typename B> constexpr auto operator/(A &&a, B &&b) {
    return binaryOp(Div{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractMatrix B>
constexpr auto operator/(std::integral auto a, B &&b) {
    return binaryOp(Div{}, a, std::forward<B>(b));
}
template <AbstractVector B>
constexpr auto operator/(std::integral auto a, B &&b) {
    return binaryOp(Div{}, a, std::forward<B>(b));
}
constexpr auto operator*(const AbstractMatrix auto &a,
                         const AbstractMatrix auto &b) {
    auto AA{a.view()};
    auto BB{b.view()};
    assert(AA.numCol() == BB.numRow());
    return MatMatMul<decltype(AA), decltype(BB)>{.a = AA, .b = BB};
}
constexpr auto operator*(const AbstractMatrix auto &a,
                         const AbstractVector auto &b) {
    auto AA{a.view()};
    auto BB{b.view()};
    assert(AA.numCol() == BB.size());
    return MatVecMul<decltype(AA), decltype(BB)>{.a = AA, .b = BB};
}
template <AbstractMatrix A>
constexpr auto operator*(A &&a, std::integral auto b) {
    return binaryOp(Mul{}, std::forward<A>(a), b);
}
// template <AbstractMatrix A> constexpr auto operator*(A &&a, Rational b) {
//     return binaryOp(Mul{}, std::forward<A>(a), b);
// }
template <AbstractVector A, AbstractVector B>
constexpr auto operator*(A &&a, B &&b) {
    return binaryOp(Mul{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractVector A>
constexpr auto operator*(A &&a, std::integral auto b) {
    return binaryOp(Mul{}, std::forward<A>(a), b);
}
// template <AbstractVector A> constexpr auto operator*(A &&a, Rational b) {
//     return binaryOp(Mul{}, std::forward<A>(a), b);
// }
template <AbstractMatrix B>
constexpr auto operator*(std::integral auto a, B &&b) {
    return binaryOp(Mul{}, a, std::forward<B>(b));
}
template <AbstractVector B>
constexpr auto operator*(std::integral auto a, B &&b) {
    return binaryOp(Mul{}, a, std::forward<B>(b));
}

// constexpr auto operator*(AbstractMatrix auto &A, AbstractVector auto &x) {
//     auto AA{A.view()};
//     auto xx{x.view()};
//     return MatMul<decltype(AA), decltype(xx)>{.a = AA, .b = xx};
// }

template <AbstractVector V>
constexpr auto operator*(const Transpose<V> &a, const AbstractVector auto &b) {
    typename V::eltype s = 0;
    for (size_t i = 0; i < b.size(); ++i)
        s += a.a(i) * b(i);
    return s;
}

static_assert(AbstractVector<Vector<int64_t>>);
static_assert(AbstractVector<const Vector<int64_t>>);
static_assert(AbstractVector<Vector<int64_t> &>);
static_assert(AbstractMatrix<IntMatrix>);
static_assert(AbstractMatrix<IntMatrix &>);

static_assert(std::copyable<Matrix<int64_t, 4, 4>>);
static_assert(std::copyable<Matrix<int64_t, 4, 0>>);
static_assert(std::copyable<Matrix<int64_t, 0, 4>>);
static_assert(std::copyable<Matrix<int64_t, 0, 0>>);
static_assert(std::copyable<SquareMatrix<int64_t>>);

static_assert(DerivedMatrix<Matrix<int64_t, 4, 4>>);
static_assert(DerivedMatrix<Matrix<int64_t, 4, 0>>);
static_assert(DerivedMatrix<Matrix<int64_t, 0, 4>>);
static_assert(DerivedMatrix<Matrix<int64_t, 0, 0>>);
static_assert(DerivedMatrix<IntMatrix>);
static_assert(DerivedMatrix<IntMatrix>);
static_assert(DerivedMatrix<IntMatrix>);

static_assert(std::is_same_v<SquareMatrix<int64_t>::eltype, int64_t>);
static_assert(std::is_same_v<IntMatrix::eltype, int64_t>);

static_assert(AbstractVector<PtrVector<Rational>>);
static_assert(AbstractVector<ElementwiseVectorBinaryOp<Sub, PtrVector<Rational>,
                                                       PtrVector<Rational>>>);

template <typename T, typename I> struct SliceView {
    using eltype = T;
    static constexpr bool canResize = false;
    [[no_unique_address]] MutPtrVector<T> a;
    [[no_unique_address]] llvm::ArrayRef<I> i;
    struct Iterator {
        [[no_unique_address]] MutPtrVector<T> a;
        [[no_unique_address]] llvm::ArrayRef<I> i;
        [[no_unique_address]] size_t j;
        bool operator==(const Iterator &k) const { return j == k.j; }
        Iterator &operator++() {
            ++j;
            return *this;
        }
        T &operator*() { return a[i[j]]; }
        const T &operator*() const { return a[i[j]]; }
        T *operator->() { return &a[i[j]]; }
        const T *operator->() const { return &a[i[j]]; }
    };
    constexpr Iterator begin() { return Iterator{a, i, 0}; }
    constexpr Iterator end() { return Iterator{a, i, i.size()}; }
    T &operator()(size_t j) { return a[i[j]]; }
    const T &operator()(size_t j) const { return a[i[j]]; }
    constexpr size_t size() const { return i.size(); }
    constexpr SliceView<T, I> view() { return *this; }
};

static_assert(AbstractVector<SliceView<int64_t, unsigned>>);
