/* inffast.h -- header to use inffast.c
 * Copyright (C) 1995-2003, 2010 Mark Adler
 * Copyright (C) 2017 ARM, Inc.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

// TODO(cblume): incorporate the patch done on crbug.com/764431 here and
// in related files to define and use INFLATE_FAST_MIN_HAVE/_LEFT etc.

void ZLIB_INTERNAL inflate_fast_chunk_ OF((z_streamp strm, unsigned start));
