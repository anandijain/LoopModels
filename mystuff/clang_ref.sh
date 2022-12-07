clang++ hello.cpp -o hello # directly makes the `hello` executable 
./hello # run the executable

clang++ -O3 -emit-llvm hello.cpp -c -o hello.bc # makes the `hello.bc` bitcode file
lli hello.bc # runs the bitcode ( actually outputs "hello world")
clang -S -emit-llvm hello.cpp -o hello.ll # https://stackoverflow.com/questions/9148890/how-to-make-clang-compile-to-llvm-ir
lli hello.ll # runs the bitcode from llvm ir ( actually outputs "hello world")
llc hello.ll -o hello.s # makes the assembly code "hello.s"
clang++ hello.s -o hello 
clang++ -c hello.s # outputs hello.o 
./hello # outputs "hello world"
clang++ hello.o -o hello_exe
./hello_exe # outputs "hello world"

