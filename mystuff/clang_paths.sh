
export MY_LLVM_PATH="/opt/homebrew/opt/llvm"
export PATH="$MY_LLVM_PATH/bin:$PATH"
export CXX=$MY_LLVM_PATH/bin/clang++
export LDFLAGS="-L$MY_LLVM_PATH/lib/c++ -Wl,-rpath,$MY_LLVM_PATH/lib/c++ -L$MY_LLVM_PATH/lib"
export CPPFLAGS="-I$MY_LLVM_PATH/include"
export CXXFLAGS="-stdlib=libc++ -std=c++20"

