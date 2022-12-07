clear && printf '\e[3J'

source clang_paths.sh
export BUILDDIR=builddirclang
meson setup $BUILDDIR # assume its new
meson test -C $BUILDDIR 
meson compile -C $BUILDDIR 
