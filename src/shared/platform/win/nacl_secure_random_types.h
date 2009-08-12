/*
 * Copyright 2008, Google Inc.
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
 * NaCl Service Runtime.  Secure RNG abstraction.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_WIN_NACL_SECURE_RANDOM_TYPES_H__
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_WIN_NACL_SECURE_RANDOM_TYPES_H__

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/shared/platform/nacl_secure_random_base.h"

#ifndef USE_CRYPTO
# define USE_CRYPTO 0
#endif

#if USE_CRYPTO
# error "We need to get AES code mapped in before this can be written/enabled"
#else

EXTERN_C_BEGIN

#define NACL_RANDOM_BUFFER_SIZE  1024

struct NaClSecureRng {
  struct NaClSecureRngIf  base;
  unsigned char           buf[NACL_RANDOM_BUFFER_SIZE];
  int                     nvalid;
};

EXTERN_C_END

#endif

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_WIN_NACL_SECURE_RANDOM_TYPES_H__ */
