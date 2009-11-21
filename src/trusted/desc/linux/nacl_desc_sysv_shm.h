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
 * NaCl service runtime.  Transferrable shared memory objects.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_LINUX_SYSV_SHM_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_LINUX_SYSV_SHM_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffector;
struct NaClDescImcShm;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct nacl_abi_timespec;
struct nacl_abi_stat;

int NaClDescSysvShmImportCtor(struct NaClDescSysvShm  *self,
                              int                     id,
                              nacl_off64_t            size);

int NaClDescSysvShmCtor(struct NaClDescSysvShm  *self,
                        nacl_off64_t            size);

void NaClDescSysvShmDtor(struct NaClDesc *vself);

uintptr_t NaClDescSysvShmMap(struct NaClDesc         *vself,
                             struct NaClDescEffector *effp,
                             void                    *start_addr,
                             size_t                  len,
                             int                     prot,
                             int                     flags,
                             nacl_off64_t            offset);

int NaClDescSysvShmUnmapUnsafe(struct NaClDesc         *vself,
                               struct NaClDescEffector *effp,
                               void                    *start_addr,
                               size_t                  len);

int NaClDescSysvShmUnmap(struct NaClDesc         *vself,
                         struct NaClDescEffector *effp,
                         void                    *start_addr,
                         size_t                  len);

int NaClDescSysvShmFstat(struct NaClDesc         *vself,
                         struct NaClDescEffector *effp,
                         struct nacl_abi_stat    *stbp);

int NaClDescSysvShmClose(struct NaClDesc         *vself,
                         struct NaClDescEffector *effp);

int NaClDescSysvShmExternalizeSize(struct NaClDesc *vself,
                                   size_t          *nbytes,
                                   size_t          *nhandles);

int NaClDescSysvShmExternalize(struct NaClDesc           *vself,
                               struct NaClDescXferState  *xfer);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_DESC_LINUX_NACL_DESC_SYSV_SHM_H_ */
