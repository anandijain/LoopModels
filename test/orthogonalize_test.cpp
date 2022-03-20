#include "../include/IntermediateRepresentation.hpp"
#include "../include/Math.hpp"
#include "../include/Symbolics.hpp"
#include "llvm/ADT/SmallVector.h"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

TEST(OrthogonalizeTest, BasicAssertions) {
    auto M = Polynomial::Monomial(Polynomial::ID{1});
    auto N = Polynomial::Monomial(Polynomial::ID{2});
    auto I = Polynomial::Monomial(Polynomial::ID{3});
    auto J = Polynomial::Monomial(Polynomial::ID{4});
    auto Zero = Polynomial::Term{intptr_t(0), Polynomial::Monomial()};
    auto One = Polynomial::Term{intptr_t(1), Polynomial::Monomial()};
    // for m = 0:M-1, n = 0:N-1, i = 0:I-1, j = 0:J-1
    //
    // Loops: m, n, i, j
    Matrix<Int, 0, 0, 0> A(4, 8);
    llvm::SmallVector<MPoly, 8> r;
    // m <= M-1
    A(0, 0) = 1;
    r.push_back(M - 1);
    // m >= 0;
    A(0, 1) = -1;
    r.push_back(Zero);
    // n <= N-1
    A(1, 2) = 1;
    r.push_back(N - 1);
    // n >= 0;
    A(1, 3) = -1;
    r.push_back(Zero);
    // i <= I-1
    A(2, 4) = 1;
    r.push_back(I - 1);
    // i >= 0;
    A(2, 5) = -1;
    r.push_back(Zero);
    // j <= J-1
    A(3, 6) = 1;
    r.push_back(J - 1);
    // j >= 0;
    A(3, 7) = -1;
    r.push_back(Zero);

    std::shared_ptr<PartiallyOrderedSet> poset =
        std::make_shared<PartiallyOrderedSet>();
    AffineLoopNestBounds alnp(
        std::make_shared<AffineLoopNest>(AffineLoopNest{A, r}), poset);

    // we have three array refs
    // W[i+m, j+n]
    llvm::SmallVector<Stride, ArrayRefPreAllocSize> Waxes;
    llvm::SmallVector<std::pair<MPoly, VarID>, 1> ipm;
    ipm.emplace_back(One, VarID(0, VarType::LoopInductionVariable));
    ipm.emplace_back(One, VarID(2, VarType::LoopInductionVariable));
    Waxes.emplace_back(One, ipm);
    llvm::SmallVector<std::pair<MPoly, VarID>, 1> jpn;
    jpn.emplace_back(One, VarID(1, VarType::LoopInductionVariable));
    jpn.emplace_back(One, VarID(3, VarType::LoopInductionVariable));
    Waxes.emplace_back(I + M - One, jpn);
    ArrayReference War(0, Waxes); //, axes, indTo
    std::cout << "War = " << War << std::endl;

    // B[i, j]
    llvm::SmallVector<Stride, ArrayRefPreAllocSize> Baxes;
    llvm::SmallVector<std::pair<MPoly, VarID>, 1> i{
        std::make_pair(One, VarID(2, VarType::LoopInductionVariable))};
    Baxes.emplace_back(One, i);
    llvm::SmallVector<std::pair<MPoly, VarID>, 1> j{
        std::make_pair(One, VarID(3, VarType::LoopInductionVariable))};
    Baxes.emplace_back(I, j);
    ArrayReference Bar(1, Baxes); //, axes, indTo
    std::cout << "Bar = " << Bar << std::endl;

    // C[m, n]
    llvm::SmallVector<Stride, ArrayRefPreAllocSize> Caxes;
    llvm::SmallVector<std::pair<MPoly, VarID>, 1> m{
        std::make_pair(One, VarID(0, VarType::LoopInductionVariable))};
    Caxes.emplace_back(One, m);
    llvm::SmallVector<std::pair<MPoly, VarID>, 1> n{
        std::make_pair(One, VarID(1, VarType::LoopInductionVariable))};
    Caxes.emplace_back(M, n);
    ArrayReference Car(2, Caxes); //, axes, indTo
    std::cout << "Car = " << Car << std::endl;

    llvm::SmallVector<ArrayReference, 0> allArrayRefs{War, Bar, Car};
    llvm::SmallVector<ArrayReference *> ai{&allArrayRefs[0], &allArrayRefs[1],
                                           &allArrayRefs[2]};

    llvm::Optional<
        std::pair<AffineLoopNestBounds, llvm::SmallVector<ArrayReference, 0>>>
        orth(orthogonalize(alnp, ai));

    EXPECT_TRUE(orth.hasValue());

    auto [newAlnp, newArrayRefs] = orth.getValue();
    std::cout << "A=" << newAlnp.aln->A << std::endl;
    // std::cout << "b=" << PtrVector<MPoly>(newAlnp.aln->b);
    EXPECT_EQ(newAlnp.lowerB[0].size(), 1);
    EXPECT_EQ(newAlnp.lowerB[1].size(), 1);
    EXPECT_EQ(newAlnp.lowerB[2].size(), 2);
    EXPECT_EQ(newAlnp.lowerB[3].size(), 2);
    EXPECT_EQ(newAlnp.upperB[0].size(), 1);
    EXPECT_EQ(newAlnp.upperB[1].size(), 1);
    EXPECT_EQ(newAlnp.upperB[2].size(), 2);
    EXPECT_EQ(newAlnp.upperB[3].size(), 2);
    std::cout << "Skewed loop nest:\n" << newAlnp << std::endl;
    std::cout << "New ArrayReferences:\n";
    for (auto &ar : newArrayRefs) {
        std::cout << ar << std::endl << std::endl;
    }
}
