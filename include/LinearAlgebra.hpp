#pragma once
#include "./Math.hpp"

struct LU {
    SquareMatrix<Rational> F;
    llvm::SmallVector<unsigned> ipiv;

    bool ldiv(MutPtrMatrix<Rational> rhs) const {
        auto [M, N] = rhs.size();
        assert(F.numRow() == M);
        // // check unimodularity
        // Rational unit = 1;
        // for (size_t i = 0; i < FM; ++i)
        //     unit *= F(i, i);
        // assert(unit == 1);

        // permute rhs
        for (size_t i = 0; i < M; ++i) {
            unsigned ip = ipiv[i];
            if (i != ip)
                for (size_t j = 0; j < M; ++j)
                    std::swap(rhs(ip, j), rhs(i, j));
        }
        // LU x = rhs
        // L y = rhs // L is UnitLowerTriangular
        for (size_t n = 0; n < N; ++n) {
            for (size_t m = 0; m < M; ++m) {
                Rational Ymn = rhs(m, n);
                for (size_t k = 0; k < m; ++k)
                    if (Ymn.fnmadd(F(m, k), rhs(k, n)))
                        return true;
                rhs(m, n) = Ymn;
            }
        }
        /*
        for (size_t k = 0; k < N; ++k) {
            for (size_t j = 0; j < M; ++j) {
                Rational rhsj = rhs(j, k);
                for (size_t i = j + 1; i < M; ++i) {
                    rhs(i, k) -= (F(i, j) * rhsj).getValue();
                }
            }
        }
        */
        // U x = y
        for (size_t n = 0; n < N; ++n) {
            for (int64_t m = M - 1; m >= 0; --m) {
                Rational Ymn = rhs(m, n);
                for (size_t k = m + 1; k < M; ++k)
                    if (Ymn.fnmadd(F(m, k), rhs(k, n)))
                        return true;
                if (auto div = Ymn.safeDiv(F(m, m))) {
                    rhs(m, n) = *div;
                } else {
                    return true;
                }
            }
        }
        return false;
    }

    bool rdiv(MutPtrMatrix<Rational> rhs) const {
        auto [M, N] = rhs.size();
        assert(F.numCol() == N);
        // // check unimodularity
        // Rational unit = 1;
        // for (size_t i = 0; i < FN; ++i)
        //     unit *= F(i, i);
        // assert(unit == 1);

        // PA = LU
        // x LU = rhs
        // y U = rhs
        for (size_t n = 0; n < N; ++n) {
            for (size_t m = 0; m < M; ++m) {
                Rational Ymn = rhs(m, n);
                for (size_t k = 0; k < n; ++k)
                    if (Ymn.fnmadd(rhs(m, k), F(k, n)))
                        return true;
                if (auto div = Ymn.safeDiv(F(n, n))) {
                    rhs(m, n) = *div;
                } else {
                    return true;
                }
            }
        }
        // x L = y
        for (int64_t n = N - 1; n >= 0; --n) {
            // for (size_t n = 0; n < N; ++n) {
            for (size_t m = 0; m < M; ++m) {
                Rational Xmn = rhs(m, n);
                for (size_t k = n + 1; k < N; ++k)
                    if (Xmn.fnmadd(rhs(m, k), F(k, n)))
                        return true;
                rhs(m, n) = Xmn;
            }
        }
        // permute rhs
        for (int64_t j = N - 1; j >= 0; --j) {
            unsigned jp = ipiv[j];
            if (j != jp)
                for (size_t i = 0; i < M; ++i)
                    std::swap(rhs(i, jp), rhs(i, j));
        }

        return false;
    }

    std::optional<SquareMatrix<Rational>> inv() const {
        SquareMatrix<Rational> A = SquareMatrix<Rational>::identity(F.numCol());
        if (!ldiv(A))
            return A;
        else
            return {};
    }
    std::optional<Rational> det() {
        Rational d = F(0, 0);
        for (size_t i = 1; i < F.numCol(); ++i)
            if (auto di = d.safeMul(F(i, i)))
                d = *di;
            else
                return {};
        return d;
    }
    llvm::SmallVector<unsigned> perm() const {
        size_t M = F.numCol();
        llvm::SmallVector<unsigned> perm;
        for (size_t m = 0; m < M; ++m) {
            perm.push_back(m);
        }
        for (size_t m = 0; m < M; ++m) {
            std::swap(perm[m], perm[ipiv[m]]);
        }
        return perm;
    }
    static llvm::Optional<LU> fact(const SquareMatrix<int64_t> &B) {
        size_t M = B.M;
        SquareMatrix<Rational> A(M);
        for (size_t m = 0; m < M * M; ++m)
            A[m] = B[m];
        llvm::SmallVector<unsigned> ipiv(M);
        for (size_t i = 0; i < M; ++i) {
            ipiv[i] = i;
        }
        for (size_t k = 0; k < M; ++k) {
            size_t kp = k;
            for (; kp < M; ++kp) {
                if (A(kp, k) != 0) {
                    ipiv[k] = kp;
                    break;
                }
            }
            if (kp != k) {
                for (size_t j = 0; j < M; ++j)
                    std::swap(A(kp, j), A(k, j));
            }
            Rational Akkinv = A(k, k).inv();
            for (size_t i = k + 1; i < M; ++i) {
                if (std::optional<Rational> Aik = A(i, k).safeMul(Akkinv))
                    A(i, k) = *Aik;
                else
                    return {};
            }
            for (size_t j = k + 1; j < M; ++j) {
                for (size_t i = k + 1; i < M; ++i) {
                    if (std::optional<Rational> Aikj =
                            A(i, k).safeMul(A(k, j))) {
                        if (std::optional<Rational> Aij =
                                A(i, j).safeSub(*Aikj)) {
                            A(i, j) = *Aij;
                            continue;
                        }
                    }
                    return {};
                }
            }
        }
        return LU{std::move(A), std::move(ipiv)};
    }
};
