#pragma once

#include "./ArrayReference.hpp"
#include "./Graphs.hpp"
#include "./Math.hpp"
#include "llvm/IR/User.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallVector.h>
#include <utility>

// We represent a schedule as
// Phi_s'*i + omega_s <_{lex} Phi_t'*s + Omega_t
// means that schedule `s` executes before schedule `t`.
//
// S_0 = {Phi_0, omega_0}
// S_1 = {Phi_1, omega_1}
// given i_0 and i_1, if
// Phi_0 * i_0 + omega_0 << Phi_1 * i_1 + omega_1
// then "i_0" for schedule "S_0" happens before
// "i_1" for schedule "S_1"
//
constexpr unsigned requiredScheduleStorage(unsigned n) { return n * (n + 2) + 1; }
// n^2 + 2n + 1-s = 0
// -1 + sqrt(1 - (1-s))
// -1 + sqrt(s)
struct Schedule {
    // given `N` loops, `P` is `N+1 x 2*N+1`
    // even rows give offsets indicating fusion (0-indexed)
    // However, all odd columns of `Phi` are structually zero,
    // so we represent it with an `N x N` matrix instead.
    static constexpr unsigned maxStackLoops = 3;
    static constexpr unsigned maxStackStorage = requiredScheduleStorage(maxStackLoops);
    // 3*3+ 2*3+1 = 16
    llvm::SmallVector<int64_t, maxStackStorage> data;
    uint8_t numLoops;
    // -1 indicates not vectorized
    int8_t vectorized{-1};
    // -1 indicates not unrolled
    // inner unroll means either the only unrolled loop, or if outer unrolled,
    // then the inner unroll is nested inside of the outer unroll.
    // if unrolledInner=3, unrolledOuter=2
    // x_0_0; x_1_0; x_2_0
    // x_0_1; x_1_1; x_2_1
    int8_t unrolledInner{-1};
    // -1 indicates not unrolled
    int8_t unrolledOuter{-1};
    void init(size_t nLoops) {
        numLoops = nLoops;
        data.resize(nLoops * (nLoops + 2) + 1);
        getPhi().antiDiag() = 1;
    }
    Schedule() = default;
    Schedule(size_t nLoops)
        : data(llvm::SmallVector<int64_t, maxStackStorage>(
              nLoops * (nLoops + 2) + 1)),
          numLoops(nLoops) {
        MutSquarePtrMatrix<int64_t> Phi(getPhi());
        Phi.antiDiag() = 1;
    };
    MutSquarePtrMatrix<int64_t> getPhi() {
        // return MutSquarePtrMatrix<int64_t>(data.data(), numLoops);
        return MutSquarePtrMatrix<int64_t>{{}, data.data(), numLoops};
    }
    MutPtrVector<int64_t> getOmega() {
        return {data.data() + numLoops * numLoops, 2 * size_t(numLoops) + 1};
    }
    SquarePtrMatrix<int64_t> getPhi() const {
        return SquarePtrMatrix<int64_t>{{}, data.data(), numLoops};
    }
    PtrVector<int64_t> getOmega() const {
        return {.mem = data.data() + numLoops * numLoops,
                .N = 2 * size_t(numLoops) + 1};
    }
    bool fusedThrough(const Schedule &y, const size_t numLoopsCommon) const {
        llvm::ArrayRef<int64_t> o0 = getOmega();
        llvm::ArrayRef<int64_t> o1 = y.getOmega();
        bool allEqual = true;
        for (size_t n = 0; n < numLoopsCommon; ++n)
            allEqual &= (o0[2 * n] == o1[2 * n]);
        return allEqual;
    }
    bool fusedThrough(const Schedule &y) const {
        return fusedThrough(y, std::min(numLoops, y.numLoops));
    }
    size_t getNumLoops() const { return numLoops; }
};


