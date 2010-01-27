/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescIoDesc subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IO_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IO_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffector;
struct NaClDescIoDesc;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct nacl_abi_stat;
struct nacl_abi_timespec;

int NaClDescIoDescCtor(struct NaClDescIoDesc  *self,
                       struct NaClHostDesc    *hd);

void NaClDescIoDescDtor(struct NaClDesc *vself);

struct NaClDescIoDesc *NaClDescIoDescMake(struct NaClHostDesc *nhdp);

/* a simple factory */
struct NaClDescIoDesc *NaClDescIoDescOpen(char  *path,
                                          int   mode,
                                          int   perms);

uintptr_t NaClDescIoDescMap(struct NaClDesc         *vself,
                            struct NaClDescEffector *effp,
                            void                    *start_addr,
                            size_t                  len,
                            int                     prot,
                            int                     flags,
                            nacl_off64_t            offset);

int NaClDescIoDescUnmapUnsafe(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp,
                              void                    *start_addr,
                              size_t                  len);

int NaClDescIoDescUnmap(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp,
                        void                    *start_addr,
                        size_t                  len);

ssize_t NaClDescIoDescRead(struct NaClDesc          *vself,
                           struct NaClDescEffector  *effp,
                           void                     *buf,
                           size_t                   len);

ssize_t NaClDescIoDescWrite(struct NaClDesc         *vself,
                            struct NaClDescEffector *effp,
                            void const              *buf,
                            size_t                  len);

nacl_off64_t NaClDescIoDescSeek(struct NaClDesc          *vself,
                                struct NaClDescEffector  *effp,
                                nacl_off64_t             offset,
                                int                      whence);

int NaClDescIoDescIoctl(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp,
                        int                     request,
                        void                    *arg);

int NaClDescIoDescFstat(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp,
                        struct nacl_abi_stat    *statbuf);

int NaClDescIoDescClose(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp);

int NaClDescIoDescExternalizeSize(struct NaClDesc *vself,
                                  size_t          *nbytes,
                                  size_t          *nhandles);

int NaClDescIoDescExternalize(struct NaClDesc           *vself,
                              struct NaClDescXferState  *xfer);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IO_H_
