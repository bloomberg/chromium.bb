#!/bin/bash
# Copyright 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
# Sets symbolic links to GCC libs from the GCC source tree (GMP, MPFR) if
# correct versions of the libraries are not present in the system.

eval "$@"

if [ ! -n "${GMP_VERSION}" ] || [ ! -n "${MPFR_VERSION}" ]; then
  echo "$0: error: provide GMP_VERSION and MPFR_VERISON"
  exit 2
fi

GNU_MP_MAJOR=0
GMP_HEADER=""
for header in gmp-i386.h gmp.h ; do
  if [[ -f /usr/include/${header} ]]; then
    declare -r GMP_HEADER=/usr/include/${header}
    break
  fi
done

if [[ ! -z ${GMP_HEADER} ]] ; then
  GNU_MP_MAJOR="`grep '#[ 	]*define[ 	]*__GNU_MP_VERSION ' ${GMP_HEADER} | sed -e s'@[ 	]*#define[ 	]*__GNU_MP_VERSION[ 	]*@@'`" ;
  GNU_MP_MINOR="`grep '#[ 	]*define[ 	]*__GNU_MP_VERSION_MINOR ' ${GMP_HEADER} | sed -e s'@[ 	]*#[ 	]*define[ 	]*__GNU_MP_VERSION_MINOR[ 	]*@@'`" ;
  GNU_MP_PATCHLEVEL="`grep '#[ 	]*define[ 	]*__GNU_MP_VERSION_PATCHLEVEL ' ${GMP_HEADER} | sed -e s'@[ 	]*[ 	]*#define[ 	]*__GNU_MP_VERSION_PATCHLEVEL[ 	]*@@'`" ;
fi

# If GMP Version < 4.3.1, use GMP from our sources.
if [[ "$GNU_MP_MAJOR" -lt 4 ]] ||
   ( [[ "$GNU_MP_MAJOR" -eq 4 ]] &&
     [[ "$GNU_MP_MINOR" -lt 3 ]] ) ||
   ( [[ "$GNU_MP_MAJOR" -eq 4 ]] &&
     [[ "$GNU_MP_MINOR" -eq 3 ]] &&
     [[ "$GNU_MP_PATCHLEVEL" -lt 1 ]] ) ; then
  echo "symlinking GMP"
  ln -sf gmp-${GMP_VERSION} SRC/gcc/gmp
else
  echo "using GMP installed in the system"
  rm -f SRC/gcc/gmp
fi

MPFR_VERSION_MAJOR=0
if [[ -f /usr/include/mpfr.h ]] ; then
  MPFR_VERSION_MAJOR="`grep '#[ 	]*define[ 	]*MPFR_VERSION_MAJOR ' /usr/include/mpfr.h | sed -e s'@[ 	]*#[ 	]*define[ 	]*MPFR_VERSION_MAJOR[ 	]*@@'`" ;
  MPFR_VERSION_MINOR="`grep '#[ 	]*define[ 	]*MPFR_VERSION_MINOR ' /usr/include/mpfr.h | sed -e s'@[ 	]*#[ 	]*define[ 	]*MPFR_VERSION_MINOR[ 	]*@@'`" ;
  MPFR_VERSION_PATCHLEVEL="`grep '#[ 	]*define[ 	]*MPFR_VERSION_PATCHLEVEL ' /usr/include/mpfr.h | sed -e s'@[ 	]*#[ 	]*define[ 	]*MPFR_VERSION_PATCHLEVEL[ 	]*@@'`" ;
fi

# If MPFR Version < 2.4.1, use GMP from our sources.
if [[ ! -e /usr/include/mpfr.h ]] ||
   [[ "$MPFR_VERSION_MAJOR" -lt 2 ]] ||
   ( [[ "$MPFR_VERSION_MAJOR" -eq 2 ]] &&
     [[ "$MPFR_VERSION_MINOR" -lt 4 ]] ) ||
   ( [[ "$MPFR_VERSION_MAJOR" -eq 2 ]] &&
     [[ "$MPFR_VERSION_MINOR" -eq 4 ]] &&
     [[ "$MPFR_VERSION_PATCHLEVEL" -lt 1 ]] ) ; then
  echo "symlinking MPFR"
  ln -sf mpfr-${MPFR_VERSION} SRC/gcc/mpfr
else
  echo "using MPFR installed in the system"
  rm -f SRC/gcc/mpfr
fi
