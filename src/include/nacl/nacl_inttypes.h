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


#ifndef NATIVE_CLIENT_SRC_INCLUDE_NACL_NACL_INTTYPES_H_
#define NATIVE_CLIENT_SRC_INCLUDE_NACL_NACL_INTTYPES_H_

#include <machine/_types.h>

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

#define NACL_PRIdS NACL__PRIS_PREFIX "d"
#define NACL_PRIiS NACL__PRIS_PREFIX "i"
#define NACL_PRIoS NACL__PRIS_PREFIX "o"
#define NACL_PRIuS NACL__PRIS_PREFIX "u"
#define NACL_PRIxS NACL__PRIS_PREFIX "x"
#define NACL_PRIXS NACL__PRIS_PREFIX "X"

# include <inttypes.h>

# define NACL_PRId32 PRId32
# define NACL_PRIi32 PRIi32
# define NACL_PRIo32 PRIo32
# define NACL_PRIu32 PRIu32
# define NACL_PRIx32 PRIx32
# define NACL_PRIX32 PRIX32

# define NACL_PRId64 PRId64
# define NACL_PRIu64 PRIu64
# define NACL_PRIx64 PRIx64
# define NACL_PRIX64 PRIX64

# define NACL_PRId16 PRId16
# define NACL_PRIu16 PRIu16
# define NACL_PRIx16 PRIx16

# define NACL_PRId8 PRId8
# define NACL_PRIu8 PRIu8
# define NACL_PRIx8 PRIx8

#endif
