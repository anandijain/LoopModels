#include "../include/ArrayReference.hpp"
#include "../include/DependencyPolyhedra.hpp"
#include "../include/LoopBlock.hpp"
#include "../include/Loops.hpp"
#include "../include/Macro.hpp"
#include "../include/Math.hpp"
#include "../include/MatrixStringParse.hpp"
#include "../include/TestUtilities.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <llvm/ADT/SmallVector.h>

TEST(DependenceTest, BasicAssertions) {

    // for (i = 0:I-2){
    //   for (j = 0:J-2){
    //     A(i+1,j+1) = A(i+1,j) + A(i,j+1);
    //   }
    // }
    // A*x >= 0;
    // [ -2  1  0 -1  0    [ 1
    //    0  0  0  1  0  *   I   >= 0
    //   -2  0  1  0 -1      J
    //    0  0  0  0  1 ]    i
    //                       j ]
    IntMatrix Aloop{stringToIntMatrix("[-2 1 0 0 -1; "
                                      "0 0 0 0 1; "
                                      "-2 0 1 -1 0; "
                                      "0 0 0 1 0]")};
    TestLoopFunction tlf;
    tlf.addLoop(std::move(Aloop), 2);
    auto &loop = tlf.alns.front();
    llvm::ScalarEvolution &SE{tlf.SE};
    llvm::Type *Int64 = tlf.builder.getInt64Ty();
    auto ptrA = tlf.getSCEVUnknown(tlf.createArray());
    // we have three array refs
    // A[i+1, j+1] // (i+1)*stride(A,1) + (j+1)*stride(A,2);
    ArrayReference Asrc(ptrA, loop, 2);
    {
        MutPtrMatrix<int64_t> IndMat = Asrc.indexMatrix();
        IndMat(1, 0) = 1; // i (loop ind: 1)
        IndMat(0, 1) = 1; // j (lopp ind: 0)
        MutPtrMatrix<int64_t> OffMat = Asrc.offsetMatrix();
        OffMat(0, 0) = 1;
        OffMat(1, 0) = 1;
        Asrc.sizes[0] = loop.S[0];
        Asrc.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "AaxesSrc = " << Asrc << "\n";

    // A[i+1, j]
    ArrayReference Atgt0(ptrA, loop, 2);
    {
        MutPtrMatrix<int64_t> IndMat = Atgt0.indexMatrix();
        IndMat(1, 0) = 1; // i
        IndMat(0, 1) = 1; // j
        Atgt0.offsetMatrix()(0, 0) = 1;
        Atgt0.sizes[0] = loop.S[0];
        Atgt0.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "AaxesTgt0 = \n" << Atgt0 << "\n";

    // A[i, j+1]
    ArrayReference Atgt1(ptrA, loop, 2);
    {
        MutPtrMatrix<int64_t> IndMat = Atgt1.indexMatrix();
        IndMat(1, 0) = 1; // i
        IndMat(0, 1) = 1; // j
        Atgt1.offsetMatrix()(1, 0) = 1;
        Atgt1.sizes[0] = loop.S[0];
        Atgt1.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "AaxesTgt1 = \n" << Atgt1 << "\n";

    //
    llvm::SmallVector<unsigned, 8> schLoad0(3);
    llvm::SmallVector<unsigned, 8> schStore(3);
    schStore[2] = 2;
    MemoryAccess msrc{Asrc, nullptr, schStore, false};
    MemoryAccess mtgt0{Atgt0, nullptr, schLoad0, true};
    SHOWLN(loop.S[0]);
    DependencePolyhedra dep0(msrc, mtgt0);
    EXPECT_FALSE(dep0.isEmpty());
    dep0.pruneBounds();
    llvm::errs() << "Dep0 = \n" << dep0 << "\n";

    EXPECT_EQ(dep0.getNumInequalityConstraints(), 4);
    EXPECT_EQ(dep0.getNumEqualityConstraints(), 2);
    assert(dep0.getNumInequalityConstraints() == 4);
    assert(dep0.getNumEqualityConstraints() == 2);

    llvm::SmallVector<unsigned, 8> schLoad1(3);
    schLoad1[2] = 1;
    MemoryAccess mtgt1{Atgt1, nullptr, schLoad1, true};
    DependencePolyhedra dep1(msrc, mtgt1);
    EXPECT_FALSE(dep1.isEmpty());
    dep1.pruneBounds();
    llvm::errs() << "Dep1 = \n" << dep1 << "\n";
    EXPECT_EQ(dep1.getNumInequalityConstraints(), 4);
    EXPECT_EQ(dep1.getNumEqualityConstraints(), 2);
    assert(dep1.getNumInequalityConstraints() == 4);
    assert(dep1.getNumEqualityConstraints() == 2);
    // MemoryAccess mtgt1{Atgt1,nullptr,schLoad,true};
    llvm::SmallVector<Dependence, 1> dc;
    EXPECT_EQ(dc.size(), 0);
    EXPECT_EQ(Dependence::check(dc, msrc, mtgt0), 1);
    EXPECT_EQ(dc.size(), 1);
    Dependence &d(dc.front());
    EXPECT_TRUE(d.forward);
    llvm::errs() << d << "\n";
    SHOWLN(d.getNumPhiCoefficients());
    SHOWLN(d.getNumOmegaCoefficients());
    SHOWLN(d.depPoly.getDim0());
    SHOWLN(d.depPoly.getDim1());
    SHOWLN(d.depPoly.getNumVar());
    SHOWLN(d.depPoly.nullStep.size());
    SHOWLN(d.depPoly.getNumSymbols());
    SHOWLN(d.depPoly.A.numCol());
    assert(d.forward);
    assert(!allZero(d.dependenceSatisfaction.tableau(
        d.dependenceSatisfaction.tableau.numRow() - 1, _)));
}

TEST(IndependentTest, BasicAssertions) {
    // symmetric copy
    // for(i = 0:I-1)
    //   for(j = 0:i-1)
    //     A(j,i) = A(i,j)
    //
    IntMatrix Aloop{stringToIntMatrix("[-1 1 0 -1; "
                                      "0 0 0 1; "
                                      "-1 0 -1 1; "
                                      "0 0 1 0]")};

    TestLoopFunction tlf;
    tlf.addLoop(std::move(Aloop), 2);
    auto &loop = tlf.alns.front();
    // loop.pruneBounds();

    llvm::ScalarEvolution &SE{tlf.SE};
    llvm::Type *Int64 = tlf.builder.getInt64Ty();
    const llvm::SCEVUnknown *scevA = tlf.getSCEVUnknown(tlf.createArray());
    // we have three array refs
    // A[i, j]
    ArrayReference Asrc(scevA, loop, 2);
    {
        MutPtrMatrix<int64_t> IndMat = Asrc.indexMatrix();
        IndMat(1, 0) = 1; // i
        IndMat(0, 1) = 1; // j
        Asrc.sizes[0] = loop.S[0];
        Asrc.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Asrc = " << Asrc << "\n";

    // A[j, i]
    ArrayReference Atgt(scevA, loop, 2);
    {
        MutPtrMatrix<int64_t> IndMat = Atgt.indexMatrix();
        IndMat(0, 0) = 1; // j
        IndMat(1, 1) = 1; // i
        Atgt.sizes[0] = loop.S[0];
        Atgt.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Atgt = " << Atgt << "\n";

    llvm::SmallVector<unsigned, 8> schLoad(3);
    llvm::SmallVector<unsigned, 8> schStore(3);
    schStore[2] = 1;
    MemoryAccess msrc{Asrc, nullptr, schStore, false};
    MemoryAccess mtgt{Atgt, nullptr, schLoad, true};
    DependencePolyhedra dep(msrc, mtgt);
    llvm::errs() << "Dep = \n" << dep << "\n";
    SHOWLN(dep.A);
    SHOWLN(dep.E);
    EXPECT_TRUE(dep.isEmpty());
    assert(dep.isEmpty());
    //
    llvm::SmallVector<Dependence, 0> dc;
    EXPECT_EQ(Dependence::check(dc, msrc, mtgt), 0);
    EXPECT_EQ(dc.size(), 0);
}
TEST(TriangularExampleTest, BasicAssertions) {
    // badly written triangular solve:
    // for (m = 0; m < M; ++m){
    //   for (n = 0; n < N; ++n){
    //     A(m,n) = B(m,n);
    //   }
    //   for (n = 0; n < N; ++n){
    //     A(m,n) /= U(n,n);
    //     for (k = n+1; k < N; ++k){
    //       A(m,k) = A(m,k) - A(m,n)*U(n,k);
    //     }
    //   }
    // }

    // Construct the loops
    IntMatrix AMN{(stringToIntMatrix("[-1 1 0 0 -1; "
                                     "0 0 0 0 1; "
                                     "-1 0 1 -1 0; "
                                     "0 0 0 1 0]"))};
    IntMatrix AMNK{(stringToIntMatrix("[-1 1 0 0 0 -1; "
                                      "0 0 0 0 0 1; "
                                      "-1 0 1 0 -1 0; "
                                      "0 0 0 0 1 0; "
                                      "-1 0 1 -1 0 0; "
                                      "-1 0 0 1 -1 0]"))};

    TestLoopFunction tlf;
    tlf.addLoop(std::move(AMN), 2);
    tlf.addLoop(std::move(AMNK), 3);
    AffineLoopNest<true> &loopMN = tlf.alns[0];
    EXPECT_FALSE(loopMN.isEmpty());
    AffineLoopNest<true> &loopMNK = tlf.alns[1];
    EXPECT_FALSE(loopMNK.isEmpty());
    const llvm::SCEV *M = loopMN.S[0];
    const llvm::SCEV *N = loopMN.S[1];
    const llvm::SCEVUnknown *scevA = tlf.getSCEVUnknown(tlf.createArray());
    const llvm::SCEVUnknown *scevB = tlf.getSCEVUnknown(tlf.createArray());
    const llvm::SCEVUnknown *scevU = tlf.getSCEVUnknown(tlf.createArray());

    // construct indices

    llvm::ScalarEvolution &SE{tlf.SE};
    llvm::Type *Int64 = tlf.builder.getInt64Ty();
    LoopBlock lblock;
    // B[m, n]
    ArrayReference BmnInd{scevB, &loopMN, 2};
    {
        MutPtrMatrix<int64_t> IndMat = BmnInd.indexMatrix();
        //     l  d
        IndMat(0, 0) = 1; // n
        IndMat(1, 1) = 1; // m
        BmnInd.sizes[0] = M;
        BmnInd.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Bmn = " << BmnInd << "\n";
    // A[n, m]
    ArrayReference Amn2Ind{scevA, loopMN, 2};
    {
        MutPtrMatrix<int64_t> IndMat = Amn2Ind.indexMatrix();
        //     l  d
        IndMat(0, 0) = 1; // n
        IndMat(1, 1) = 1; // m
        Amn2Ind.sizes[0] = M;
        Amn2Ind.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Amn2 = " << Amn2Ind << "\n";
    // A[n, m]
    ArrayReference Amn3Ind{scevA, loopMNK, 2};
    {
        MutPtrMatrix<int64_t> IndMat = Amn3Ind.indexMatrix();
        //     l  d
        IndMat(1, 0) = 1; // n
        IndMat(2, 1) = 1; // m
        Amn3Ind.sizes[0] = M;
        Amn3Ind.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Amn3 = " << Amn3Ind << "\n";
    // A[k, m]
    ArrayReference AmkInd{scevA, loopMNK, 2};
    {
        MutPtrMatrix<int64_t> IndMat = AmkInd.indexMatrix();
        //     l  d
        IndMat(0, 0) = 1; // k
        IndMat(2, 1) = 1; // m
        AmkInd.sizes[0] = M;
        AmkInd.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Amk = " << AmkInd << "\n";
    // U[k, n]
    ArrayReference UnkInd{scevU, loopMNK, 2};
    {
        MutPtrMatrix<int64_t> IndMat = UnkInd.indexMatrix();
        //     l  d
        IndMat(1, 1) = 1; // n
        IndMat(0, 0) = 1; // k
        UnkInd.sizes[0] = N;
        UnkInd.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Unk = " << UnkInd << "\n";
    // U[n, n]
    ArrayReference UnnInd{scevU, loopMN, 2};
    {
        MutPtrMatrix<int64_t> IndMat = UnnInd.indexMatrix();
        //     l  d
        IndMat(0, 1) = 1; // n
        IndMat(0, 0) = 1; // n
        UnnInd.sizes[0] = N;
        UnnInd.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Unn = " << UnnInd << "\n";

    // for (m = 0; m < M; ++m){
    //   for (n = 0; n < N; ++n){
    //     // sch.Omega = [ 0, _, 0, _, {0-1} ]
    //     A(m,n) = B(m,n); // sch2_0_{0-1}
    //   }
    //   for (n = 0; n < N; ++n){
    //     // sch.Omega = [ 0, _, 1, _, {0-2} ]
    //     A(m,n) = A(m,n) / U(n,n); // sch2_2_{0-2}
    //     for (k = n+1; k < N; ++k){
    //       // sch.Omega = [ 0, _, 1, _, 3, _, {0-3} ]
    //       A(m,k) = A(m,k) - A(m,n)*U(n,k); // sch3_{0-3}
    //     }
    //   }
    //   foo(arg...) // [ 0, _, 2 ]
    // }
    // NOTE: shared ptrs get set to NULL when `lblock.memory` reallocs...
    lblock.memory.reserve(9);
    llvm::SmallVector<unsigned, 8> sch2_0_0(2 + 1);
    llvm::SmallVector<unsigned, 8> sch2_0_1 = sch2_0_0;
    // A(m,n) = -> B(m,n) <-
    MemoryAccess mSch2_0_0(BmnInd, nullptr, sch2_0_0, true);
    lblock.memory.push_back(&mSch2_0_0);
    sch2_0_1[2] = 1;
    llvm::SmallVector<unsigned, 8> sch2_1_0 = sch2_0_1;
    // -> A(m,n) <- = B(m,n)
    MemoryAccess mSch2_0_1(Amn2Ind, nullptr, sch2_0_1, false);
    lblock.memory.push_back(&mSch2_0_1);
    sch2_1_0[1] = 1;
    sch2_1_0[2] = 0;
    llvm::SmallVector<unsigned, 8> sch2_1_1 = sch2_1_0;
    // A(m,n) = -> A(m,n) <- / U(n,n); // sch2
    MemoryAccess mSch2_1_0(Amn2Ind, nullptr, sch2_1_0, true);
    lblock.memory.push_back(&mSch2_1_0);
    sch2_1_1[2] = 1;
    llvm::SmallVector<unsigned, 8> sch2_1_2 = sch2_1_1;
    // A(m,n) = A(m,n) / -> U(n,n) <-;
    MemoryAccess mSch2_1_1(UnnInd, nullptr, sch2_1_1, true);
    lblock.memory.push_back(&mSch2_1_1);
    sch2_1_2[2] = 2;
    // -> A(m,n) <- = A(m,n) / U(n,n); // sch2
    MemoryAccess mSch2_1_2(Amn2Ind, nullptr, sch2_1_2, false);
    lblock.memory.push_back(&mSch2_1_2);

    llvm::SmallVector<unsigned, 8> sch3_0(3 + 1);
    sch3_0[1] = 1;
    sch3_0[2] = 3;
    llvm::SmallVector<unsigned, 8> sch3_1 = sch3_0;
    // A(m,k) = A(m,k) - A(m,n)* -> U(n,k) <-;
    MemoryAccess mSch3_2(UnkInd, nullptr, sch3_0, true);
    lblock.memory.push_back(&mSch3_2);
    sch3_1[3] = 1;
    llvm::SmallVector<unsigned, 8> sch3_2 = sch3_1;
    // A(m,k) = A(m,k) - -> A(m,n) <- *U(n,k);
    MemoryAccess mSch3_1(Amn3Ind, nullptr, sch3_1, true);
    lblock.memory.push_back(&mSch3_1);
    sch3_2[3] = 2;
    llvm::SmallVector<unsigned, 8> sch3_3 = sch3_2;
    // A(m,k) = -> A(m,k) <- - A(m,n)*U(n,k);
    MemoryAccess mSch3_0(AmkInd, nullptr, sch3_2, true);
    lblock.memory.push_back(&mSch3_0);
    sch3_3[3] = 3;
    // -> A(m,k) <- = A(m,k) - A(m,n)*U(n,k);
    MemoryAccess mSch3_3(AmkInd, nullptr, sch3_3, false);
    lblock.memory.push_back(&mSch3_3);
    EXPECT_EQ(lblock.memory.size(), 9);

    // for (m = 0; m < M; ++m){
    //   for (n = 0; n < N; ++n){
    //     A(m,n) = B(m,n); // sch2_0_{0-1}
    //   }
    //   for (n = 0; n < N; ++n){
    //     A(m,n) = A(m,n) / U(n,n); // sch2_2_{0-2}
    //     for (k = n+1; k < N; ++k){
    //       A(m,k) = A(m,k) - A(m,n)*U(n,k); // sch3_{0-3}
    //     }
    //   }
    // }

    // First, comparisons of store to `A(m,n) = B(m,n)` versus...
    llvm::SmallVector<Dependence, 0> d;
    d.reserve(16);
    llvm::SmallVector<Dependence, 0> r;
    r.reserve(16);
    // // load in `A(m,n) = A(m,n) / U(n,n)`
    EXPECT_EQ(Dependence::check(d, mSch2_0_1, mSch2_1_0), 1);
    EXPECT_EQ(Dependence::check(r, mSch2_1_0, mSch2_0_1), 1);
    EXPECT_TRUE(d.back().forward);
    EXPECT_FALSE(r.back().forward);
    // dep#1
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    //
    //
    // store in `A(m,n) = A(m,n) / U(n,n)`
    EXPECT_EQ(Dependence::check(d, mSch2_0_1, mSch2_1_2), 1);
    EXPECT_EQ(Dependence::check(r, mSch2_1_2, mSch2_0_1), 1);
    EXPECT_TRUE(d.back().forward);
    EXPECT_FALSE(r.back().forward);
    // dep#2
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";

    //
    // sch3_               3        0         1     2
    // load `A(m,n)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'

    EXPECT_EQ(Dependence::check(d, mSch2_0_1, mSch3_1), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_1, mSch2_0_1), 1);
    EXPECT_TRUE(d.back().forward);
    EXPECT_FALSE(r.back().forward);
    // dep#3
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    // load `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    //
    EXPECT_EQ(Dependence::check(d, mSch2_0_1, mSch3_0), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_0, mSch2_0_1), 1);
    EXPECT_TRUE(d.back().forward);
    EXPECT_FALSE(r.back().forward);
    // dep#4
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    // store `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch2_0_1, mSch3_3), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_3, mSch2_0_1), 1);
    EXPECT_TRUE(d.back().forward);
    EXPECT_FALSE(r.back().forward);
    // dep#5
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    EXPECT_EQ(d.size(), 5);
    EXPECT_EQ(r.size(), 5);

    // Second, comparisons of load in `A(m,n) = A(m,n) / U(n,n)`
    // with...
    // store in `A(m,n) = A(m,n) / U(n,n)`
    EXPECT_EQ(Dependence::check(d, mSch2_1_0, mSch2_1_2), 1);
    EXPECT_EQ(Dependence::check(r, mSch2_1_2, mSch2_1_0), 1);
    EXPECT_TRUE(d.back().forward);
    EXPECT_FALSE(r.back().forward);
    // dep#6
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    //
    // sch3_               3        0         1     2
    // load `A(m,n)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch2_1_0, mSch3_1), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_1, mSch2_1_0), 1);
    EXPECT_TRUE(d.back().forward);
    EXPECT_FALSE(r.back().forward);
    // dep#7
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    // load `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch2_1_0, mSch3_0), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_0, mSch2_1_0), 1);
    EXPECT_FALSE(d.back().forward);
    EXPECT_TRUE(r.back().forward);
    // dep#8
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    // NOTE: these are two load-load comparisons!
    // Hence, `fillEdges()` will currently not add these!!
    // store `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch2_1_0, mSch3_3), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_3, mSch2_1_0), 1);
    EXPECT_FALSE(d.back().forward);
    EXPECT_TRUE(r.back().forward);
    // dep#9
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";

    // Third, comparisons of store in `A(m,n) = A(m,n) / U(n,n)`
    // with...
    // sch3_               3        0         1     2
    // load `A(m,n)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch2_1_2, mSch3_1), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_1, mSch2_1_2), 1);
    EXPECT_TRUE(d.back().forward);
    EXPECT_FALSE(r.back().forward);
    // dep#10
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    // load `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch2_1_2, mSch3_0), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_0, mSch2_1_2), 1);
    EXPECT_FALSE(d.back().forward);
    EXPECT_TRUE(r.back().forward);
    // dep#11
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    // store `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch2_1_2, mSch3_3), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_3, mSch2_1_2), 1);
    EXPECT_FALSE(d.back().forward);
    EXPECT_TRUE(r.back().forward);
    // dep#12
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";

    // Fourth, comparisons of load `A(m,n)` in
    // sch3_               3        0         1     2
    // load `A(m,n)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    // with...
    // load `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch3_1, mSch3_0), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_0, mSch3_1), 1);
    EXPECT_FALSE(d.back().forward);
    EXPECT_TRUE(r.back().forward);
    // dep#13
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";
    // NOTE: this is another load-load comparison that fillEdges
    // will not add currently!
    // store `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    EXPECT_EQ(Dependence::check(d, mSch3_1, mSch3_3), 1);
    EXPECT_EQ(Dependence::check(r, mSch3_3, mSch3_1), 1);
    EXPECT_FALSE(d.back().forward);
    EXPECT_TRUE(r.back().forward);
    // dep#14
    llvm::errs() << "dep#" << d.size() << ":\n" << d.back() << "\n";

    // Fifth, comparisons of load `A(m,k)` in
    // sch3_               3        0         1     2
    // load `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    // with...
    // store `A(m,k)` in 'A(m,k) = A(m,k) - A(m,n)*U(n,k)'
    // printMatrix(llvm::errs() << "mSch3_0.schedule.getPhi() =\n",
    // PtrMatrix<const int64_t>(mSch3_0.schedule.getPhi())) << "\n";
    // printMatrix(llvm::errs()
    // << "mSch3_3.schedule.getPhi() =\n", PtrMatrix<const
    // int64_t>(mSch3_3.schedule.getPhi())) << "\n"; printVector(llvm::errs()
    // << "mSch3_0.schedule.getOmega() = ", mSch3_0.schedule.getOmega()) <<
    // "\n"; printVector(llvm::errs() << "mSch3_3.schedule.getOmega() = ",
    // mSch3_3.schedule.getOmega()) << "\n";
    EXPECT_EQ(Dependence::check(d, mSch3_0, mSch3_3), 2);
    EXPECT_EQ(Dependence::check(r, mSch3_3, mSch3_0), 2);
    EXPECT_TRUE(d[d.size() - 2].forward);
    EXPECT_FALSE(d[d.size() - 1].forward);
    EXPECT_FALSE(r[r.size() - 2].forward);
    EXPECT_TRUE(r[r.size() - 1].forward);
    // dep#16
    llvm::errs() << "dep#" << d.size() << "\n";
    auto &forward = d[d.size() - 2];
    auto &reverse = d[d.size() - 1];
    llvm::errs() << "\nforward dependence:" << forward;
    llvm::errs() << "\nreverse dependence:" << reverse;
    assert(forward.forward);
    assert(!reverse.forward);
    EXPECT_EQ(d.size(), 16);
    EXPECT_EQ(r.size(), 16);
    // EXPECT_EQ(forward.dependenceSatisfaction.getNumConstraints(), 3);
    // EXPECT_EQ(reverse.dependenceSatisfaction.getNumConstraints(), 2);
    // EXPECT_EQ(forward.dependenceSatisfaction.getNumInequalityConstraints(),
    // 2); EXPECT_EQ(forward.dependenceSatisfaction.getNumEqualityConstraints(),
    // 1);
    // EXPECT_EQ(reverse.dependenceSatisfaction.getNumInequalityConstraints(),
    // 1); EXPECT_EQ(reverse.dependenceSatisfaction.getNumEqualityConstraints(),
    // 1);
    EXPECT_TRUE(allZero(forward.depPoly.E(_, 0)));
    EXPECT_FALSE(allZero(reverse.depPoly.E(_, 0)));
    int nonZeroInd = -1;
    for (unsigned i = 0; i < reverse.depPoly.E.numRow(); ++i) {
        bool notZero = !allZero(reverse.depPoly.getEqSymbols(i));
        // we should only find 1 non-zero
        EXPECT_FALSE((nonZeroInd != -1) & notZero);
        if (notZero)
            nonZeroInd = i;
    }
    // v_1 is `n` for the load
    // v_4 is `n` for the store
    // thus, we expect v_1 = v_4 + 1
    // that is, the load depends on the store from the previous iteration
    // (e.g., store when `v_4 = 0` is loaded when `v_1 = 1`.
    auto nonZero = reverse.depPoly.getCompTimeEqOffset(nonZeroInd);
    const size_t numSymbols = reverse.depPoly.getNumSymbols();
    EXPECT_EQ(numSymbols, 3);
    EXPECT_TRUE(nonZero.hasValue());
    if (nonZero.getValue() == 1) {
        // v_1 - v_4 == 1
        // 1 - v_1 + v_4 == 0
        EXPECT_EQ(reverse.depPoly.E(nonZeroInd, numSymbols + 1), -1);
        EXPECT_EQ(reverse.depPoly.E(nonZeroInd, numSymbols + 4), 1);
    } else {
        // -v_1 + v_4 == -1
        // -1 + v_1 - v_4 == 0
        EXPECT_EQ(nonZero.getValue(), -1);
        EXPECT_EQ(reverse.depPoly.E(nonZeroInd, numSymbols + 1), 1);
        EXPECT_EQ(reverse.depPoly.E(nonZeroInd, numSymbols + 4), -1);
    }
    //
    lblock.fillEdges();
    // EXPECT_FALSE(lblock.optimize());
    // -3 comes from the fact we did 3 load-load comparisons above
    // in the future, we may have `fillEdges` make load-load comparisons
    // so that we can add bounding constraints to the objective, to
    // favor putting repeated loads close together.
    // However, we would not add the scheduling constraints.
    EXPECT_EQ(lblock.edges.size(), d.size() - 3);
    // llvm::errs() << "Number of edges found: " << lblock.edges.size() <<
    // "\n"; EXPECT_EQ(lblock.edges.size(), 12); for (auto &e :
    // lblock.edges) {
    //    llvm::errs() << "Edge:\n" << e << "\n" << "\n";
    //}
}

TEST(RankDeficientLoad, BasicAssertions) {

    // for (i = 0:I-1){
    //   for (j = 0:i){
    //     A(i,j) = A(i,i);
    //   }
    // }
    // A*x <= b
    // [ 1   0     [i        [ I - 1
    //  -1   0   *  j ]        0
    //  -1   1           <=    0
    //   0  -1 ]               0     ]
    //
    IntMatrix Aloop{stringToIntMatrix("[-1 1 0 -1; "
                                      "0 0 0 1; "
                                      "0 0 -1 1; "
                                      "0 0 1 0]")};
    TestLoopFunction tlf;
    tlf.addLoop(std::move(Aloop), 2);
    auto &loop = tlf.alns.front();
    llvm::ScalarEvolution &SE{tlf.SE};
    llvm::Type *Int64 = tlf.builder.getInt64Ty();
    const llvm::SCEVUnknown *scevA = tlf.getSCEVUnknown(tlf.createArray());

    // we have three array refs
    // A[i, j] // i*stride(A,1) + j*stride(A,2);
    ArrayReference Asrc(scevA, loop, 2);
    {
        MutPtrMatrix<int64_t> IndMat = Asrc.indexMatrix();
        IndMat(1, 0) = 1; // i
        IndMat(0, 1) = 1; // j
        Asrc.sizes[0] = loop.S[0];
        Asrc.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "AaxesSrc = " << Asrc << "\n";

    // A[i, i]
    ArrayReference Atgt(scevA, loop, 2);
    {
        MutPtrMatrix<int64_t> IndMat = Atgt.indexMatrix();
        IndMat(1, 0) = 1; // i
        IndMat(1, 1) = 1; // i
        Atgt.sizes[0] = loop.S[0];
        Atgt.sizes[1] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "AaxesTgt = \n" << Atgt << "\n";

    llvm::SmallVector<unsigned, 8> schLoad(2 + 1);
    llvm::SmallVector<unsigned, 8> schStore(2 + 1);
    schStore[2] = 1;
    MemoryAccess msrc{Asrc, nullptr, schStore, false};
    MemoryAccess mtgt{Atgt, nullptr, schLoad, true};

    llvm::SmallVector<Dependence, 1> deps;
    EXPECT_EQ(Dependence::check(deps, msrc, mtgt), 1);
    EXPECT_FALSE(deps.back().forward); // load -> store
    llvm::errs() << "Blog post example:\n" << deps[0] << "\n";
}

TEST(TimeHidingInRankDeficiency, BasicAssertions) {
    // for (i = 0; i < I; ++i)
    //   for (j = 0; j < J; ++j)
    //     for (k = 0; k < K; ++k)
    //       A(i+j, j+k, i-k) = foo(A(i+j, j+k, i-k));
    //
    // Indexed by three LIVs, and three dimensional
    // but memory access pattern is only rank 2, leaving
    // a time dimension of repeated memory accesses.
    // A*x <= b
    // [ 1   0  0     [i        [ I - 1
    //  -1   0  0   *  j          0
    //   0   1  0      k ]    <=  J - 1
    //   0  -1  0 ]               0
    //   0   0  1 ]               K - 1
    //   0   0 -1 ]               0     ]
    //
    IntMatrix Aloop{stringToIntMatrix("[-1 1 0 0 0 0 -1; "
                                      "0 0 0 0 0 0 1; "
                                      "-1 0 1 0 0 -1 0; "
                                      "0 0 0 0 0 1 0; "
                                      "-1 0 0 1 -1 0 0; "
                                      "0 0 0 0 1 0 0]")};
    TestLoopFunction tlf;
    tlf.addLoop(std::move(Aloop), 3);
    auto &loop = tlf.alns.front();
    llvm::ScalarEvolution &SE{tlf.SE};
    llvm::Type *Int64 = tlf.builder.getInt64Ty();

    const llvm::SCEV *I = loop.S[0];
    const llvm::SCEV *J = loop.S[1];
    const llvm::SCEV *K = loop.S[2];
    const llvm::SCEVUnknown *scevA = tlf.getSCEVUnknown(tlf.createArray());

    // we have three array refs
    // A[i+j, j+k, i - k]
    ArrayReference Aref(scevA, loop, 3);
    {
        MutPtrMatrix<int64_t> IndMat = Aref.indexMatrix();
        IndMat(2, 0) = 1;  // i
        IndMat(1, 0) = 1;  // + j
        IndMat(1, 1) = 1;  // j
        IndMat(0, 1) = 1;  // + k
        IndMat(2, 2) = 1;  // i
        IndMat(0, 2) = -1; // -k
        Aref.sizes[0] = SE.getAddExpr(J, K);
        Aref.sizes[1] = SE.getAddExpr(I, K);
        Aref.sizes[2] = SE.getConstant(Int64, 8, /*isSigned=*/false);
    }
    llvm::errs() << "Aref = " << Aref << "\n";

    llvm::SmallVector<unsigned, 8> schLoad(3 + 1);
    llvm::SmallVector<unsigned, 8> schStore(3 + 1);
    schStore[3] = 1;
    MemoryAccess msrc{Aref, nullptr, schStore, false};
    MemoryAccess mtgt{Aref, nullptr, schLoad, true};

    llvm::SmallVector<Dependence, 2> deps;
    EXPECT_EQ(Dependence::check(deps, msrc, mtgt), 2);
    assert(deps.size() == 2);
    llvm::errs() << "Rank deficicient example:\nForward:\n"
                 << deps[0] << "\nReverse:\n"
                 << deps[1] << "\n";
}
