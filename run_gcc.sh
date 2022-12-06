clear && printf '\e[3J'

source gcc_paths.sh
export BUILDDIR=builddirgcc6
meson setup $BUILDDIR # assume its new
meson test -C $BUILDDIR 
meson compile -C $BUILDDIR 
