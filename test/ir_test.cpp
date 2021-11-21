#include "../include/math.hpp"
#include "../include/ir.hpp"
#include <cstdio>
#include <gtest/gtest.h>

TEST(IRTest, BasicAssertions) {
    EXPECT_EQ(3, 3);
    Const a = Const{.type = Int64, .bits = 0};
    showln(a);
    a = Const{.type = Float64, .bits = 0x4002666666666666};
    showln(a);
    a = Const{.type = Float32, .bits = 0x0000000040133333};
    showln(a);
    // pretty print
    // showln(ArrayRef ar);
    // We'll build an ArrayRef
    // i_2 (Induction Variable) +
    // 2 M_0 i_8 (Memory) +
    // (3 M_0 M_1) i_18 (Term) +
    // (5 + 7 M_0) i_3 (Induction Variable) +
    // (11 + 13 (M_0 M_2) + 17 (M_0 M_1 M_2)) i_0 (Induction Variable)
    std::vector<SourceType> indTyp({LOOPINDUCTVAR, MEMORY, TERM, LOOPINDUCTVAR, LOOPINDUCTVAR});
    std::vector<size_t> indID({2, 8, 18, 3, 0});
    std::vector<Int> coef_memory({1, 2, 3, 5, 7, 11, 13, 17});
    std::vector<size_t> coef_offsets({0, 1, 2, 3, 5, 8});
    VoV<Int> coef = VoV<Int>(toVector(coef_memory), toVector(coef_offsets));
    std::vector<size_t> pvc_memory({0, 0, 1, 0, 0, 2, 0, 1, 2});
    std::vector<size_t> innerOffsets({0, 0, 0, 1, 0, 2, 0, 0, 1, 0, 0, 2, 5});
    // printf("innOff len: %d\n", innerOffsets.size());
    std::vector<size_t> outerOffsets({0, 2, 4, 6, 9, 13});
    showln(toVector(innerOffsets));
    size_t raw[16];
    Vector<size_t, 0> memBuffer(raw, outerOffsets.size());
    // std::vector<size_t> memBuffer().resize(outerOffsets.size());
    VoVoV<size_t> pvc = VoVoV<size_t>(&pvc_memory.front(), toVector(innerOffsets), toVector(outerOffsets), memBuffer);

    ArrayRef ar = ArrayRef{ .arrayID = 10, .indTyp = toVector(indTyp), .indID = toVector(indID), .programVariableCombinations = pvc, .coef = coef };
    showln(ar);
}
