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

#include "native_client/src/trusted/desc/nacl_desc_base.h"

EXTERN_C_BEGIN

struct NaClDescEffector;
struct NaClDescXferState;
struct NaClMessageHeader;
struct nacl_abi_stat;

/*
 * Since the SysV shared memory abstraction does not permit mapping in
 * only a portion of the shared memory object (offset==0), the SysV
 * shm objects have their own fstat object type.  The user code should
 * not encounter SysV shm objects normally and is unable to create any
 * SysV shm objects; the only source of SysV shm objects is in the
 * context of Pepper2d, and the Pepper2d library code should hide
 * this implementation detail.
 */
extern struct NaClDescVtbl const kNaClDescSysvShmVtbl;

struct NaClDescSysvShm {
  struct NaClDesc           base;
  int                       id;
  size_t                    size;
};

extern int NaClDescSysvShmInternalize(struct NaClDesc          **baseptr,
                                      struct NaClDescXferState *xfer) NACL_WUR;


int NaClDescSysvShmImportCtor(struct NaClDescSysvShm  *self,
                              int                     id,
                              nacl_off64_t            size) NACL_WUR;

int NaClDescSysvShmCtor(struct NaClDescSysvShm  *self,
                        nacl_off64_t            size) NACL_WUR;

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_DESC_LINUX_NACL_DESC_SYSV_SHM_H_ */
