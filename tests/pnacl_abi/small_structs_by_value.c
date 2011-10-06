/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * tests whether small structures are lowered as expected
 */

typedef struct { char x; char y; } TWO_CHARS;
typedef struct { short x; short y; } TWO_SHORTS;
typedef struct { int x; int y; } TWO_INTS;

/* @IGNORE_LINES_FOR_CODE_HYGIENE[3] */
extern void foo_chars(TWO_CHARS z);
extern void foo_shorts(TWO_SHORTS z);
extern void foo_ints(TWO_INTS z);

void bar_chars(TWO_CHARS z) {
  foo_chars(z);
}

void bar_shorts(TWO_SHORTS z) {
  foo_shorts(z);
}

void bar_ints(TWO_INTS z) {
  foo_ints(z);
}
