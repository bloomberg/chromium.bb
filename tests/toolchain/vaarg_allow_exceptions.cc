/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <cassert>
#include <cstdarg>

// On NaCl targets int64s and doubles use alignment of 8 instead of 4
// (the equivalent of the -malign-double flag), whereas on x86-32 Linux they
// have alignment of 4. However the alignment of double arguments passed on
// the stack is still 4 in all cases. With le32 bitcode, the frontend leaves
// va_arg calls as intrinsics which are expanded by the -expand-varargs pass
// at link time. With x86-32-native bitcode, the frontend expands them itself.
// When using --pnacl-allow-exceptions, the normal ABI simplification passes
// are not run, but -expand-varargs must be run or this test will fail because
// the backend will use 8-byte-aligned load to expand the va_arg intrinsic.

double getit(int z, ...) {
  va_list ap;
  va_start(ap, z);
  return va_arg(ap, double);
}

typedef double (*F) (int, ...);
volatile F f = getit;

int main(void) {
  double ret = f(1, 42.5);
  assert(ret == 42.5);
  return 0;
}
