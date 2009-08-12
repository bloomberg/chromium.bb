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
 * NaCl Service Runtime.  Secure RNG.  Global singleton.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_GLOBAL_SECURE_RANDOM_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_GLOBAL_SECURE_RANDOM_H_

#include "native_client/src/shared/platform/nacl_secure_random.h"
#include "native_client/src/shared/platform/nacl_sync.h"

void NaClGlobalSecureRngInit(void);

void NaClGlobalSecureRngSwitchRngForTesting(struct NaClSecureRng *);

void NaClGlobalSecureRngFini(void);

int32_t NaClGlobalSecureRngUniform(int32_t range_max);

/*
 * Generate a random alpha-numeric name for a socket, a semaphore or some
 * other device.
 */
void NaClGenerateRandomPath(char *path, int length);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_GLOBAL_SECURE_RANDOM_H_ */
