#include "../Include/Macro.hpp"
#include <gtest/gtest.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <cassert>  
// #include <std::ranges>

template <typename T>

void add1(T &x) {
   ++x;
}

TEST(IR_SUITE, IR_TESTS) {
    auto ctx = llvm::LLVMContext();
    auto BB = llvm::BasicBlock::Create(ctx);
    auto builder = llvm::IRBuilder(BB);
    // auto zero = llvm::Value(0);
    auto zero = builder.getInt64(0);
    auto one = builder.getInt64(1);
    // assert(false);
    // SHOWLN(&builder);
    auto s = "hello world";

    // int i = 0;
    // auto list2 = {0, 1  };
    // auto list3 = {0, 0};
    // std::transform(mylist.begin(), mylist.end(), list3, add1);
    
    // auto mylist = std::vector(0, 1);
    std::vector<int> mylist{0,1,2,3};
    auto l2 = mylist;
    // std::ranges::transform(mylist, mylist.begin(), add1<int>);
    auto new_list = std::for_each(mylist.begin(), mylist.end(), add1<int>);

    // map(x->2x, [1,2,3])
    // std::vector<int> new_list 
    llvm::SmallVector<int64_t, 100>();
    SHOWLN(list4);

    
    // std::transform(&builder.getInt64, mylist);
    // std::string a = "abc";
    EXPECT_EQ(1, 1);
}
