#include "../include/NormalForm.hpp"
#include "../include/Polyhedra.hpp"
#include "Orthogonalize.hpp"
#include "llvm/ADT/SmallVector.h"
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>

static void BM_NullSpace(benchmark::State &state) {

    IntMatrix B(6, 4);
    B(0, 0) = 1;
    B(1, 0) = 0;
    B(2, 0) = -3;
    B(3, 0) = 0;
    B(4, 0) = 2;
    B(5, 0) = -8;

    B(0, 1) = 0;
    B(1, 1) = 1;
    B(2, 1) = 5;
    B(3, 1) = 0;
    B(4, 1) = -1;
    B(5, 1) = 4;

    B(0, 2) = 0;
    B(1, 2) = 0;
    B(2, 2) = 0;
    B(3, 2) = 1;
    B(4, 2) = 7;
    B(5, 2) = -9;

    // fourth row is 0
    // std::cout << "B=\n" << B << "\nnullSpace(B) =\n" <<
    // NormalForm::nullSpace(B) << std::endl;
    IntMatrix A;
    for (auto _ : state)
        A = NormalForm::nullSpace(B);
}
// Register the function as a benchmark
BENCHMARK(BM_NullSpace);

static void BM_NullSpace2000(benchmark::State &state) {
    const size_t N = 20;
    IntMatrix A(N, N);
    A(0, 0) = 2;
    for (size_t i = 1; i < N; ++i) {
        A(i - 1, i) = -1;
        A(i, i) = 2;
        A(i, i - 1) = -1;
    }
    for (size_t j = 0; j < N; j += 8) {
        // A(j,:)
        for (size_t i = 0; i < N; ++i) {
            A(j, i) = 0;
        }
        for (size_t i = 0; i < N; i += 7) {
            int64_t s = (i & 1) ? 1 : -1;
            for (size_t k = 0; k < N; ++k) {
                A(j, k) += s * A(i, k);
            }
        }
    }

    // fourth row is 0
    IntMatrix NS;
    for (auto _ : state) {
        NS = NormalForm::nullSpace(A);
    }
    // std::cout << "NS.size() = (" << NS.numRow() << ", " << NS.numCol() << ")"
    //           << std::endl;
}
// Register the function as a benchmark
BENCHMARK(BM_NullSpace2000);

static void BM_Orthogonalize(benchmark::State &state) {
    IntMatrix A(7, 7);
    IntMatrix B;
    A(1, 1) = -2;
    A(1, 2) = 2;
    A(1, 3) = 0;
    A(1, 4) = 1;
    A(1, 5) = 1;
    A(1, 6) = 1;
    A(1, 7) = 2;
    A(2, 1) = 3;
    A(2, 2) = -3;
    A(2, 3) = 2;
    A(2, 4) = 3;
    A(2, 5) = 2;
    A(2, 6) = 3;
    A(2, 7) = 2;
    A(3, 1) = -3;
    A(3, 2) = 0;
    A(3, 3) = 2;
    A(3, 4) = 3;
    A(3, 5) = -2;
    A(3, 6) = 0;
    A(3, 7) = 1;
    A(4, 1) = 2;
    A(4, 2) = 1;
    A(4, 3) = 0;
    A(4, 4) = -1;
    A(4, 5) = 3;
    A(4, 6) = -1;
    A(4, 7) = 1;
    A(5, 1) = 1;
    A(5, 2) = -3;
    A(5, 3) = -3;
    A(5, 4) = -2;
    A(5, 5) = 2;
    A(5, 6) = -2;
    A(5, 7) = 2;
    A(6, 1) = 0;
    A(6, 2) = 0;
    A(6, 3) = 1;
    A(6, 4) = 2;
    A(6, 5) = -3;
    A(6, 6) = -2;
    A(6, 7) = -2;
    A(7, 1) = 0;
    A(7, 2) = -3;
    A(7, 3) = -2;
    A(7, 4) = -1;
    A(7, 5) = 1;
    A(7, 6) = 0;
    A(7, 7) = 1;
    for (auto _ : state) {
        B = orthogonalize(A);
    }
}
BENCHMARK(BM_Orthogonalize);

static void BM_Bareiss2000(benchmark::State &state) {
    const size_t N = 20;
    IntMatrix A(N, N);
    A(0, 0) = 2;
    for (size_t i = 1; i < N; ++i) {
        A(i - 1, i) = -1;
        A(i, i) = 2;
        A(i, i - 1) = -1;
    }
    for (size_t j = 0; j < N; j += 8) {
        // A(j,:)
        for (size_t i = 0; i < N; ++i) {
            A(j, i) = 0;
        }
        for (size_t i = 0; i < N; i += 7) {
            int64_t s = (i & 1) ? 1 : -1;
            for (size_t k = 0; k < N; ++k) {
                A(j, k) += s * A(i, k);
            }
        }
    }
    // std::cout << A << std::endl;

    // fourth row is 0
    llvm::SmallVector<size_t, 16> pivots;
    pivots.reserve(N);
    IntMatrix B;
    for (auto _ : state) {
        pivots.clear();
        B = A;
        NormalForm::bareiss(B, pivots);
    }
    // std::cout << "NS.size() = (" << NS.numRow() << ", " << NS.numCol() << ")"
    //           << std::endl;
}
// Register the function as a benchmark
BENCHMARK(BM_Bareiss2000);

BENCHMARK_MAIN();
