./autogen.sh &&
export CPPFLAGS="-I/$HOME/build/win32/libyaml/include/" &&
export LDFLAGS="-L/$HOME/build/win32/libyaml/lib/" &&
./configure --host i586-mingw32msvc $ENABLE_UCS4 --with-yaml --prefix=$(pwd)/out-mingw32-install &&
make LDFLAGS="$LDFLAGS -avoid-version -Xcompiler -static-libgcc"

# FIXME:
# make CPPFLAGS='-I/$HOME/build/win32/libyaml/include/' \
#      LDFLAGS='-L/$HOME/build/win32/libyaml/lib/' \
#      distwin32
