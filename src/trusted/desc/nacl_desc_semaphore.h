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
 * NaCl service runtime.  NaClDescSemaphore subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SEMAPHORE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SEMAPHORE_H_

#include "native_client/src/include/portability.h"

struct NaClDesc;
struct NaClDescEffector;
struct NaClHostDesc;
struct NaClDescXferState;
struct NaClDescSemaphore;
struct NaClMessageHeader;
struct nacl_abi_stat;
struct nacl_abi_timespec;

int NaClDescSemaphoreCtor(struct NaClDescSemaphore  *self, int value);

void NaClDescSemaphoreDtor(struct NaClDesc *vself);

int NaClDescSemaphoreClose(struct NaClDesc          *vself,
                           struct NaClDescEffector  *effp);

int NaClDescSemaphorePost(struct NaClDesc         *vself,
                          struct NaClDescEffector *effp);

int NaClDescSemaphoreSemWait(struct NaClDesc          *vself,
                             struct NaClDescEffector  *effp);

int NaClDescSemaphoreGetValue(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp);

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SEMAPHORE_H_
