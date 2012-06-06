#!/bin/bash
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

##################################################################
#  Description: tool to examine an x86_64 linux system to look for
#  missing packages needed to develop for Native Client and help the
#  user to install any that are missing.  This makes many assumptions
#  about the linux distribution (Ubuntu) and version (Hardy Heron) and
#  might not work for other distributions/versions.
##################################################################

PATH=/usr/local/sbin:/usr/sbin:/sbin:/usr/local/bin:/usr/bin:/bin

umask=077
d=${TMPDIR:-/tmp}/nacl64.$$
if ! mkdir "$d" > /dev/null 2>&1
then
  cat >&2 << EOF
Could not create safe temporary directory "$d".

ABORTING.
EOF
  exit 1
fi
f="$d/x.c"
fout="$d/x"
trap 'rm -fr "$d"; exit' 0 1 2 3 15

function isRunningAsRoot() {
  whoami | grep -q 'root'
}

function try() {
  cat > "$f" << EOF
#include <openssl/aes.h>
int main(void) {
  AES_KEY	k;
  char		key[AES_BLOCK_SIZE];

  AES_set_encrypt_key(key, AES_BLOCK_SIZE * 8, &k);
  return 0;
}
EOF
  if ! gcc -o "$fout" "$f" -lssl -lcrypto > /dev/null 2>&1
  then
    return 1
  fi
  return 0
}

function ensure_installed {
  if ! [ -e "$1" ]
  then
    cat >&2 << EOF
... you do not have $2.  Installing...
EOF
    if apt-get -y install "$2" > /dev/null 2>&1
    then
      echo "... done" >&2
    else
      cat >&2 <<EOF
... failed to install $2.

ABORTING
EOF
      exit 1
    fi
  fi
}

if ! isRunningAsRoot
then
  cat >&2 << \EOF
Not running as root, so cannot install libraries/links.
Note: you probably will need to copy this script to the local file system
(and off of NFS) in order to run this script as root.

ABORTING.
EOF
  exit 1
fi

# openssl is required for both 32 and 64-bit systems.
ensure_installed '/usr/include/openssl/aes.h' 'libssl-dev'
# gtk-2.0 is required for both 32 and 64-bit systems
ensure_installed '/usr/include/gtk-2.0/gtk/gtk.h' 'libgtk2.0-dev'
# expat is needed by gdb
ensure_installed '/usr/include/expat.h' 'libexpat-dev'

cat > "$f" << \EOF
int main(void) { return sizeof(long) == 8; }
EOF
gcc -o "$fout" "$f" > /dev/null 2>&1

if "$fout"
then
  cat << \EOF
You do not appear to be using an x86_64 system.  This rest of this script
is not required.
EOF
  exit 0
fi

ensure_installed '/usr/lib32' 'ia32-libs'
ensure_installed '/usr/lib32/libncurses.a' 'lib32ncurses5-dev'
ensure_installed '/usr/lib/gcc/x86_64-linux-gnu/4.4/32/libstdc++.so' 'g++-multilib'

# now check for symlinks

function do_symlink {
  declare -a vers

  target="/usr/lib32/$1"

  if ! [ -f "$target" ]
  then
    vers=($(echo "$target".*))
    if (( ${#vers[@]} != 1 ))
    then
      cat >&2 << EOF
More than one version of $target.* exists.  This script will
picks the lexicographically greatest one.
EOF
      vers=("${vers[${#vers[@]}-1]}")
      echo "Using $vers" >&2
    fi
    cat >&2 <<EOF
Linking $target to $vers
EOF
    ln -s "$vers" "$target"
  fi
}

declare -a dev_libs
dev_libs=(libatk-1.0.so libgtk-x11-2.0.so libgdk-x11-2.0.so \
  libgdk_pixbuf-2.0.so libpangocairo-1.0.so libpango-1.0.so \
  libcairo.so libgobject-2.0.so libgmodule-2.0.so libglib-2.0.so \
  libXt.so libX11.so)

for lib in "${dev_libs[@]}"
do
  do_symlink "$lib"
done

if [ -f /usr/lib32/libcrypto.so -a -f /usr/lib32/libssl.so ] && try
then
  echo "You already have /usr/lib32/libcrypto.so and libssl.so"
else

# we omit libssl.so and libcrypto.so from dev_libs so we can ensure
# that ssl_vers is consistent between the two.

  declare -a ssl_libs
  ssl_libs=($(echo /usr/lib32/libssl.so.*))
  if (( ${#ssl_libs[@]} != 1 ))
  then
    cat >&2 << \EOF
WARNING:  More than one versions of /usr/lib32/libssl.so.*  This script will
pick the lexicographically greatest one.
EOF
    ssl_libs=(${ssl_libs[${#ssl_libs[@]}-1]})
    echo "Using $ssl_libs" >&2
  fi
  ssl_lib=${ssl_libs[0]}

  ssl_vers=$(expr \( $ssl_lib : '/usr/lib32/libssl\.so\.\(.*\)' \))
  case "$ssl_vers" in
    ''|'*')
      cat >&2 << \EOF
/usr/lib32/libssl.so.SSLVERSION missing.  This should have been installed
as part of the ia32-libs package.

ABORTING.
EOF
      exit 1
      ;;
  esac
  if ! [ -f /usr/lib32/libcrypto.so.$ssl_vers ]
  then
    cat >&2 << EOF
Version mismatch:  /usr/lib32/libssl.so.$ssl_vers exists, but
/usr/lib32/libcrypto.so.$ssl_vers does not!

ABORTING.
EOF
    exit 1
  fi
  echo "SSL version $ssl_vers shared libraries are available"

  echo "Attempting to install ia32-libs-dev to get crypto libraries..." >&2
  apt-get -y install ia32-libs-dev > /dev/null 2>&1
  if [ -f /usr/lib32/libcrypto.so -a -f /usr/lib32/libssl.so ] && try
  then
    echo "Done."
    exit 0
  fi
  cat >&2 << \EOF
... failed.

Fallback: attempting to install lib32bz2-dev to get crypto libraries...
EOF
  apt-get -y install lib32bz2-dev > /dev/null 2>&1
  if [ -f /usr/lib32/libcrypto.so -a -f /usr/lib32/libssl.so ] && try
  then
    echo "Done."
    exit 0
  fi
  cat >&2 << \EOF
... failed.

Fallback: installing symlinks manually....
EOF
  if ! ln -s libcrypto.so.$ssl_vers /usr/lib32/libcrypto.so
  then
    cat >&2 << \EOF
... failed.  Could not make symbolic links in /usr/lib32.

ABORTING.
EOF
    exit 1
  fi
  ln -s libssl.so.$ssl_vers /usr/lib32/libssl.so

  if try
  then
    echo "Success."
  else
    echo "FAILED." >&2
    exit 1
  fi
fi
