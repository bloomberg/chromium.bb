
/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescDirDesc subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_DIR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_DIR_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffector;
struct NaClDescDirDesc;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct nacl_abi_stat;
struct nacl_abi_timespec;

int NaClDescDirDescCtor(struct NaClDescDirDesc  *self,
                        struct NaClHostDir      *hd);

void NaClDescDirDescDtor(struct NaClDesc *vself);

struct NaClDescDirDesc *NaClDescDirDescMake(struct NaClHostDir *nhdp);

/* simple factory */
struct NaClDescDirDesc *NaClDescDirDescOpen(char  *path);

ssize_t NaClDescDirDescRead(struct NaClDesc         *vself,
                            struct NaClDescEffector *effp,
                            void                    *buf,
                            size_t                  len);

int NaClDescDirDescClose(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp);

ssize_t NaClDescDirDescGetdents(struct NaClDesc         *vself,
                                struct NaClDescEffector *effp,
                                void                    *dirp,
                                size_t                  count);

int NaClDescDirDescExternalizeSize(struct NaClDesc *vself,
                                   size_t          *nbytes,
                                   size_t          *nhandles);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_DIR_H_
