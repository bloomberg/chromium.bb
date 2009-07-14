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

#ifndef NATIVE_CLIENT_SRC_SHARED_UTILS_TYPES_H__
#define NATIVE_CLIENT_SRC_SHARED_UTILS_TYPES_H__

#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

/* Note: the names Bool, FALSE and TRUE are defined in some directories,
 * which conflict with native_client/src/include/portability.h. Hence,
 * this file has not been encorporated into that header file.
 */

/* Define an implementation for boolean values. */
#if NACL_WINDOWS
typedef BOOL Bool;
#else
typedef enum {
  FALSE,
  TRUE
} Bool;
#endif

/*
 * It doesn't really make much sense to have all the diouxX formats, but
 * we include them for completeness/orthogonality.
 */
#define PRIdBool  "d"
#define PRIiBool  "i"
#define PRIoBool  "o"
#define PRIuBool  "u"
#define PRIxBool  "x"
#define PRIXBool  "X"

EXTERN_C_END

#endif   /* NATIVE_CLIENT_SRC_SHARED_UTILS_TYPES_H__ */
