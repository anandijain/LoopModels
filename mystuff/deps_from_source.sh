# Check out the library.
# export MY_DEPS_DIR="$HOME/code/cpp/deps"
# git clone https://github.com/google/benchmark.git
# cd benchmark
cmake -E make_directory "build"
cmake -E chdir "build" cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
cmake --build "build" --config Release
sudo cmake --build "build" --config Release --target install
sudo make install