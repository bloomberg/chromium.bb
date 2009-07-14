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
 * NaCl Service Runtime.  Secure RNG abstraction.  Base class.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SECURE_RANDOM_BASE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SECURE_RANDOM_BASE_H_

#include "native_client/src/include/portability.h"

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

struct NaClSecureRngIf;  /* fwd: base interface class */

struct NaClSecureRngVtbl {
  void      (*Dtor)(struct NaClSecureRngIf  *self);
  /*
   * Generates a uniform random 8-bit byte (uint8_t).
   */
  uint8_t   (*GenByte)(struct NaClSecureRngIf *self);
  /*
   * Generates a uniform random 32-bit unsigned int.
   */
  uint32_t  (*GenUint32)(struct NaClSecureRngIf *self);
  /*
   * Generate an uniformly random number in [0, range_max).  May invoke
   * the provided generator multiple times.
   */
  void      (*GenBytes)(struct NaClSecureRngIf  *self,
                        uint8_t                 *buf,
                        size_t                  nbytes);
  uint32_t  (*Uniform)(struct NaClSecureRngIf *self,
                       uint32_t               range_max);
};

struct NaClSecureRngIf {
  struct NaClSecureRngVtbl const  *vtbl;
};

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SECURE_RANDOM_BASE_H_ */
