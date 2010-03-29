/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  Transferrable shared memory objects.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IMC_SHM_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IMC_SHM_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_base.h"

/*
 * get NaClHandle, which is a typedef and not a struct pointer, so
 * impossible to just forward declare.
 */
#include "native_client/src/shared/imc/nacl_imc_c.h"

/* get nacl_off64_t */
#include "native_client/src/shared/platform/nacl_host_desc.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffector;
struct NaClDescImcShm;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct nacl_abi_timespec;
struct nacl_abi_stat;

/*
 * Constructor: initialize the NaClDescImcShm object based on the
 * underlying low-level IMC handle h which has the given size.
 */
int NaClDescImcShmCtor(struct NaClDescImcShm  *self,
                       NaClHandle             h,
                       nacl_off64_t           size);

/*
 * A convenience wrapper for above, where the shm object of a given
 * size is allocated first.
 */
int NaClDescImcShmAllocCtor(struct NaClDescImcShm  *self,
                            nacl_off64_t           size);

void NaClDescImcShmDtor(struct NaClDesc *vself);

struct NaClDescImcShm *NaClDescImcShmMake(struct NaClHostDesc *nhdp);

uintptr_t NaClDescImcShmMap(struct NaClDesc         *vself,
                            struct NaClDescEffector *effp,
                            void                    *start_addr,
                            size_t                  len,
                            int                     prot,
                            int                     flags,
                            nacl_off64_t            offset);

int NaClDescImcShmUnmapUnsafe(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp,
                              void                    *start_addr,
                              size_t                  len);

int NaClDescImcShmUnmap(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp,
                        void                    *start_addr,
                        size_t                  len);

int NaClDescImcShmFstat(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp,
                        struct nacl_abi_stat    *stbp);

int NaClDescImcShmClose(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp);

int NaClDescImcShmExternalizeSize(struct NaClDesc *vself,
                                  size_t          *nbytes,
                                  size_t          *nhandles);

int NaClDescImcShmExternalize(struct NaClDesc           *vself,
                              struct NaClDescXferState  *xfer);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IMC_SHM_H_
