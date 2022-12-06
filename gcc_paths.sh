
export MY_LLVM_PATH="/opt/homebrew/opt/llvm"
export MY_GCC_CELLAR_PATH="/opt/homebrew/Cellar/gcc/12.2.0"
export PATH="$MY_LLVM_PATH/bin:$PATH"

export CC=gcc-12 # for gtest, not needed for loopmodels
export CXX=g++-12

export LDFLAGS="-L$MY_GCC_CELLAR_PATH/lib"
export CPPFLAGS="-I$MY_GCC_CELLAR_PATH/include"
export CXXFLAGS="-stdlib=libstdc++"

