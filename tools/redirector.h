/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TOOLS_REDIRECTOR_H_
#define NATIVE_CLIENT_TOOLS_REDIRECTOR_H_

#include <wchar.h>
typedef wchar_t* redirect_t[3];
redirect_t redirects[] = {
  {L"/bin/nacl-addr2line.exe", L"/libexec/nacl64-addr2line.exe", L""},
  {L"/bin/nacl-ar.exe", L"/libexec/nacl64-ar.exe", L""},
  {L"/bin/nacl-as.exe", L"/libexec/nacl64-as.exe", L"--32"},
  {L"/bin/nacl-c++.exe", L"/libexec/nacl64-c++.exe", L"-m32"},
  {L"/bin/nacl-c++filt.exe", L"/libexec/nacl64-c++filt.exe", L""},
  {L"/bin/nacl-cpp.exe", L"/libexec/nacl64-cpp.exe", L""},
  {L"/bin/nacl-g++.exe", L"/libexec/nacl64-g++.exe", L"-m32"},
  {L"/bin/nacl-gcc.exe", L"/libexec/nacl64-gcc.exe", L"-m32"},
  {L"/bin/nacl-gcc-4.4.3.exe", L"/libexec/nacl64-gcc-4.4.3.exe", L"-m32"},
  {L"/bin/nacl-gccbug.exe", L"/libexec/nacl64-gccbug.exe", L""},
  {L"/bin/nacl-gcov.exe", L"/libexec/nacl64-gcov.exe", L""},
  {L"/bin/nacl-gfortran.exe", L"/libexec/nacl64-gfortran.exe", L"-m32"},
  {L"/bin/nacl-gprof.exe", L"/libexec/nacl64-gprof.exe", L""},
  {L"/bin/nacl-ld.exe", L"/libexec/nacl64-ld.exe", L"-melf_nacl"},
  {L"/bin/nacl-nm.exe", L"/libexec/nacl64-nm.exe", L""},
  {L"/bin/nacl-objcopy.exe", L"/libexec/nacl64-objcopy.exe", L""},
  {L"/bin/nacl-objdump.exe", L"/libexec/nacl64-objdump.exe", L""},
  {L"/bin/nacl-ranlib.exe", L"/libexec/nacl64-ranlib.exe", L""},
  {L"/bin/nacl-readelf.exe", L"/libexec/nacl64-readelf.exe", L""},
  {L"/bin/nacl-size.exe", L"/libexec/nacl64-size.exe", L""},
  {L"/bin/nacl-strings.exe", L"/libexec/nacl64-strings.exe", L""},
  {L"/bin/nacl-strip.exe", L"/libexec/nacl64-strip.exe", L""},
  {L"/bin/nacl64-addr2line.exe", L"/libexec/nacl64-addr2line.exe", L""},
  {L"/bin/nacl64-ar.exe", L"/libexec/nacl64-ar.exe", L""},
  {L"/bin/nacl64-as.exe", L"/libexec/nacl64-as.exe", L""},
  {L"/bin/nacl64-c++.exe", L"/libexec/nacl64-c++.exe", L"-m64"},
  {L"/bin/nacl64-c++filt.exe", L"/libexec/nacl64-c++filt.exe", L""},
  {L"/bin/nacl64-cpp.exe", L"/libexec/nacl64-cpp.exe", L""},
  {L"/bin/nacl64-g++.exe", L"/libexec/nacl64-g++.exe", L"-m64"},
  {L"/bin/nacl64-gcc.exe", L"/libexec/nacl64-gcc.exe", L"-m64"},
  {L"/bin/nacl64-gcc-4.4.3.exe", L"/libexec/nacl64-gcc-4.4.3.exe", L"-m64"},
  {L"/bin/nacl64-gccbug.exe", L"/libexec/nacl64-gccbug.exe", L""},
  {L"/bin/nacl64-gcov.exe", L"/libexec/nacl64-gcov.exe", L""},
  {L"/bin/nacl64-gfortran.exe", L"/libexec/nacl64-gfortran.exe", L"-m64"},
  {L"/bin/nacl64-gprof.exe", L"/libexec/nacl64-gprof.exe", L""},
  {L"/bin/nacl64-ld.exe", L"/libexec/nacl64-ld.exe", L""},
  {L"/bin/nacl64-nm.exe", L"/libexec/nacl64-nm.exe", L""},
  {L"/bin/nacl64-objcopy.exe", L"/libexec/nacl64-objcopy.exe", L""},
  {L"/bin/nacl64-objdump.exe", L"/libexec/nacl64-objdump.exe", L""},
  {L"/bin/nacl64-ranlib.exe", L"/libexec/nacl64-ranlib.exe", L""},
  {L"/bin/nacl64-readelf.exe", L"/libexec/nacl64-readelf.exe", L""},
  {L"/bin/nacl64-size.exe", L"/libexec/nacl64-size.exe", L""},
  {L"/bin/nacl64-strings.exe", L"/libexec/nacl64-strings.exe", L""},
  {L"/bin/nacl64-strip.exe", L"/libexec/nacl64-strip.exe", L""},
  {L"/nacl64/bin/ar.exe", L"/libexec/nacl64-ar.exe", L""},
  {L"/nacl64/bin/as.exe", L"/libexec/nacl64-as.exe", L""},
  {L"/nacl64/bin/c++.exe", L"/libexec/nacl64-c++.exe", L"-m64"},
  {L"/nacl64/bin/gcc.exe", L"/libexec/nacl64-gcc.exe", L"-m64"},
  {L"/nacl64/bin/g++.exe", L"/libexec/nacl64-g++.exe", L"-m64"},
  {L"/nacl64/bin/gfortran.exe", L"/libexec/nacl64-gfortran.exe", L"-m64"},
  {L"/nacl64/bin/ld.exe", L"/libexec/nacl64-ld.exe", L""},
  {L"/nacl64/bin/nm.exe", L"/libexec/nacl64-nm.exe", L""},
  {L"/nacl64/bin/objcopy.exe", L"/libexec/nacl64-objcopy.exe", L""},
  {L"/nacl64/bin/objdump.exe", L"/libexec/nacl64-objdump.exe", L""},
  {L"/nacl64/bin/ranlib.exe", L"/libexec/nacl64-ranlib.exe", L""},
  {L"/nacl64/bin/strip.exe", L"/libexec/nacl64-strip.exe", L""},
};
#endif
