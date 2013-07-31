/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/toolchain/pic_constant_lib.h"

/*
 * This is a regression test for:
 *     https://code.google.com/p/nativeclient/issues/detail?id=3549
 * The compiler bug affected -fPIC compilations only.
 */

static const char *h(const char *s, size_t n) {
  return n != 0 ? g(s, n) : s;
}

static const char *f(const char *s) {
  return h(s, g(s, 0) - s);
}

const char *i(void) {
  return f("string constant");
}
