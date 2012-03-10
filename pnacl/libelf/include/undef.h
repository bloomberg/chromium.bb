/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __UNDEF_H
#define __UNDEF_H

/* powerof2(x) = Is x a power of 2?
   libelf uses this. It is provided by glibc, but not newlib. */
#undef powerof2
#define powerof2(x)      ((((x) - 1) & (x)) == 0)

/* libelf references these, but never uses them
   since we don't ask it to do file I/O.
   Since Newlib does not provide them, we need these
   macros to prevent undefined references during linking. */
#undef fcntl
#undef ftruncate
#undef fchmod
#undef pread
#undef pwrite
#define fcntl(...)       (0)
#define ftruncate(...)   (0)
#define fchmod(...)      (0)
#define pread(...)       (0)
#define pwrite(...)      (0)

#endif
