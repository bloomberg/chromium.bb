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
 * PRI*32 the last time and may be included through multiple paths.
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

#define PRIdS NACL__PRIS_PREFIX "d"
#define PRIiS NACL__PRIS_PREFIX "i"
#define PRIoS NACL__PRIS_PREFIX "o"
#define PRIuS NACL__PRIS_PREFIX "u"
#define PRIxS NACL__PRIS_PREFIX "x"
#define PRIXS NACL__PRIS_PREFIX "X"

/*
 * Newlib stdint.h defines int32_t as unsigned long int for nacl, while
 * newlib inttypes.h seems to think that int32_t is unsigned int.
 */

#ifdef PRId32
#undef PRId32
#endif
#define PRId32 "ld"

#ifdef PRIi32
#undef PRIi32
#endif
#define PRIi32 "li"

#ifdef PRIo32
#undef PRIo32
#endif
#define PRIo32 "lo"

#ifdef PRIu32
#undef PRIu32
#endif
#define PRIu32 "lu"

#ifdef PRIx32
#undef PRIx32
#endif
#define PRIx32 "lx"

#ifdef PRIX32
#undef PRIX32
#endif
#define PRIX32 "lX"
