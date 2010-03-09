/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * We do not want an include guard on this file, as this file needs to define
 * NACL_PRI*32 the last time and may be included through multiple paths.
 */

/*
 * printf macros for size_t, in the style of inttypes.h.  This is
 * needed since the gcc and newlib header files don't have definitions
 * for size_t.
 */
#ifndef NACL__PRIS_PREFIX
# if defined(__STRICT_ANSI__)
#  define  NACL__PRIS_PREFIX "l"
# else
#  define  NACL__PRIS_PREFIX
# endif
#endif

#ifdef NACL_PRIdS
#undef NACL_PRIdS
#endif
#define NACL_PRIdS NACL__PRIS_PREFIX "d"

#ifdef NACL_PRIiS
#undef NACL_PRIiS
#endif
#define NACL_PRIiS NACL__PRIS_PREFIX "i"

#ifdef NACL_PRIoS
#undef NACL_PRIoS
#endif
#define NACL_PRIoS NACL__PRIS_PREFIX "o"

#ifdef NACL_PRIuS
#undef NACL_PRIuS
#endif
#define NACL_PRIuS NACL__PRIS_PREFIX "u"

#ifdef NACL_PRIxS
#undef NACL_PRIxS
#endif
#define NACL_PRIxS NACL__PRIS_PREFIX "x"

#ifdef NACL_PRIXS
#undef NACL_PRIXS
#endif
#define NACL_PRIXS NACL__PRIS_PREFIX "X"

/*
 * Newlib stdint.h defines int32_t as unsigned long int for nacl, while
 * newlib inttypes.h seems to think that int32_t is unsigned int.
 */

#ifdef NACL_PRId32
#undef NACL_PRId32
#endif
#define NACL_PRId32 "ld"

#ifdef NACL_PRIi32
#undef NACL_PRIi32
#endif
#define NACL_PRIi32 "li"

#ifdef NACL_PRIo32
#undef NACL_PRIo32
#endif
#define NACL_PRIo32 "lo"

#ifdef NACL_PRIu32
#undef NACL_PRIu32
#endif
#define NACL_PRIu32 "lu"

#ifdef NACL_PRIx32
#undef NACL_PRIx32
#endif
#define NACL_PRIx32 "lx"

#ifdef NACL_PRIX32
#undef NACL_PRIX32
#endif
#define NACL_PRIX32 "lX"

#ifdef NACL_PRId64
#undef NACL_PRId64
#endif
#define NACL_PRId64 "lld"

#ifdef NACL_PRIu64
#define NACL_PRIu64
#endif
#define NACL_PRIu64 "llu"

#ifdef NACL_PRIx64
#undef NACL_PRIx64
#endif
#define NACL_PRIx64 "llx"

#ifdef NACL_PRIX64
#undef NACL_PRIX64
#endif
#define NACL_PRIX64 "llX"

#ifdef NACL_PRId16
#undef NACL_PRId16
#endif
#define NACL_PRId16 "d"

#ifdef NACL_PRIu16
#undef NACL_PRIu16
#endif
#define NACL_PRIu16 "u"

#ifdef NACL_PRIx16
#undef NACL_PRIx16
#endif
#define NACL_PRIx16 "x"

#ifdef NACL_PRId8
#undef NACL_PRId8
#endif
#define NACL_PRId8 "d"

#ifdef NACL_PRIu8
#undef NACL_PRIu8
#endif
#define NACL_PRIu8 "u"

#ifdef NACL_PRIx8
#undef NACL_PRIx8
#endif
#define NACL_PRIx8 "x"
