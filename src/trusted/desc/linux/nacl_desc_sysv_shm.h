/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
