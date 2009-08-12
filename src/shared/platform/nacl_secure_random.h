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

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SECURE_RANDOM_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SECURE_RANDOM_H_

#include "native_client/src/include/portability.h"

/*
 * Get struct NaClSecureRng, since users will need its size for
 * placement-new style initialization.
 * TODO(bsy): is there another way to do this?
 */
#if NACL_LINUX || NACL_OSX
# include "native_client/src/shared/platform/linux/nacl_secure_random_types.h"
#elif NACL_WINDOWS
# include "native_client/src/shared/platform/win/nacl_secure_random_types.h"
#endif

EXTERN_C_BEGIN

void NaClSecureRngModuleInit(void);

void NaClSecureRngModuleFini(void);

int NaClSecureRngCtor(struct NaClSecureRng *self);

/*
 * This interface is for TESTING ONLY.  Having user-provided seed
 * material can be dangerous, since the available entropy in the seed
 * material is unknown.  The interface does not define the desired
 * seed material size -- in part, to discourage non-testing uses.
 *
 * Alternate subclasses may also be used for testing; this interface
 * exercises the actual RNG code, albeit with a possibly poor/fixed
 * key.
 */
int NaClSecureRngTestingCtor(struct NaClSecureRng *self,
                             uint8_t              *seed_material,
                             size_t               seed_bytes);

/*
 * Default implementations for subclasses.  Generally speakly,
 * probably shouldn't call directly -- just use the vtbl.
 */
uint32_t NaClSecureRngDefaultGenUint32(struct NaClSecureRngIf *self);

void NaClSecureRngDefaultGenBytes(struct NaClSecureRngIf  *self,
                                  uint8_t                 *buf,
                                  size_t                  nbytes);

uint32_t NaClSecureRngDefaultUniform(struct NaClSecureRngIf *self,
                                     uint32_t               range_max);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SECURE_RANDOM_H_ */
