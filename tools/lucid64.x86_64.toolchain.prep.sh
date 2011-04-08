# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PATH=/usr/local/sbin:/usr/sbin:/sbin:/usr/local/bin:/usr/bin:/bin
umask 077

set -x
set -e
set -u

declare -A libraries=(
  [libppl0.10-dev]=p/ppl
  [libcloog-ppl-dev]=c/cloog-ppl
  [libgmp-dev]=g/gmp
  [libmpfr-dev]=m/mpfr4
  [libmpc-dev]=m/mpclib
)

declare -a mirrors=(
  http://mirrors.kernel.org/ubuntu/pool/main
  http://mirrors.kernel.org/debian/pool/main
)

working_directory="${TMPDIR:-/tmp}/nacl64.$$"

mkdir "$working_directory"

cd "$working_directory"

if dpkg -s libppl0.11-dev ; then
  libraries[libppl0.11-dev]=${libraries[libppl0.10-dev]}
  unset libraries[libppl0.10-dev]
fi

if ((EUID==0)); then
  install=1
  apt-get install "${!libraries[@]}"
else
  echo "If libraries are not all installed please do:"
  echo "  sudo apt-get install ${!libraries[@]}"
  install=0
fi
for library in "${!libraries[@]}"; do
  dpkg -s "$library" | grep Version |
  {
    read skip version
    if [[ "$version" = *:* ]] ; then
      version="${version/*:/}"
      echo $version
    fi
    found=0
    for mirror in "${mirrors[@]}"; do
      if wget "$mirror/${libraries[$library]}/${library}_${version}_i386.deb"
      then
        found=1
      fi
    done
    if ((!found)); then
      exit 1
    fi
    ar x "${library}_${version}_i386.deb" data.tar.gz
    tar xSvpf data.tar.gz --wildcards './usr/lib/*.*a'
    rm data.tar.gz
  }
done
perl -pi -e s'|/usr/lib|/usr/lib32|' usr/lib/*.la
mv usr/lib/libgmpxx.a usr/lib/libgmpxx-orig.a
cat <<END
OUTPUT_FORMAT(elf32-i386)
GROUP ( /usr/lib32/libgmpxx-orig.a
        AS_NEEDED ( /usr/lib32/libstdc++.so /usr/lib32/libm.so ) )
END
if ((install)); then
  mv -f usr/lib/*.*a /usr/lib32
else
  echo "Move files from $PWD/usr/lib to /usr/lib32"
fi
