/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.  Memory
 * mapping using descriptors.
 */

#include "native_client/src/include/portability.h"

#if NACL_WINDOWS
# include "io.h"
# include "fcntl.h"
#endif

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/shared/platform/nacl_find_addrsp.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"


/*
 * This file contains the implementation for the NaClIoDesc subclass
 * of NaClDesc.
 *
 * NaClDescIoDesc is the subclass that wraps host-OS descriptors
 * provided by NaClHostDesc (which gives an OS-independent abstraction
 * for host-OS descriptors).
 */

static struct NaClDescVtbl const kNaClDescIoDescVtbl;  /* fwd */

static int NaClDescIoDescSubclassCtor(struct NaClDescIoDesc  *self,
                                      struct NaClHostDesc    *hd) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  self->hd = hd;
  basep->base.vtbl = (struct NaClRefCountVtbl const *) &kNaClDescIoDescVtbl;
  return 1;
}

/*
 * Takes ownership of hd, will close in Dtor.
 */
int NaClDescIoDescCtor(struct NaClDescIoDesc  *self,
                       struct NaClHostDesc    *hd) {
  struct NaClDesc *basep = (struct NaClDesc *) self;
  int rv;

  basep->base.vtbl = (struct NaClRefCountVtbl const *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  rv = NaClDescIoDescSubclassCtor(self, hd);
  if (!rv) {
    (*NACL_VTBL(NaClRefCount, basep)->Dtor)((struct NaClRefCount *) basep);
  }
  return rv;
}

static void NaClDescIoDescDtor(struct NaClRefCount *vself) {
  struct NaClDescIoDesc *self = (struct NaClDescIoDesc *) vself;

  NaClLog(4, "NaClDescIoDescDtor(0x%08"NACL_PRIxPTR").\n",
          (uintptr_t) vself);
  NaClHostDescClose(self->hd);
  free(self->hd);
  self->hd = NULL;
  vself->vtbl = (struct NaClRefCountVtbl const *) &kNaClDescVtbl;
  (*vself->vtbl->Dtor)(vself);
}

struct NaClDescIoDesc *NaClDescIoDescMake(struct NaClHostDesc *nhdp) {
  struct NaClDescIoDesc *ndp;

  ndp = malloc(sizeof *ndp);
  if (NULL == ndp) {
    NaClLog(LOG_FATAL,
            "NaClDescIoDescMake: no memory for 0x%08"NACL_PRIxPTR"\n",
            (uintptr_t) nhdp);
  }
  if (!NaClDescIoDescCtor(ndp, nhdp)) {
    NaClLog(LOG_FATAL,
            ("NaClDescIoDescMake:"
             " NaClDescIoDescCtor(0x%08"NACL_PRIxPTR",0x%08"NACL_PRIxPTR
             ") failed\n"),
            (uintptr_t) ndp,
            (uintptr_t) nhdp);
  }
  return ndp;
}

struct NaClDesc *NaClDescIoDescMakeFromHandle(NaClHandle handle) {
  int posix_d;
  struct NaClHostDesc *nhdp;
  struct NaClDescIoDesc *desc;

#if NACL_WINDOWS
  posix_d = _open_osfhandle((intptr_t) handle, _O_RDWR | _O_BINARY);
  if (-1 == posix_d)
    return NULL;
#else
  posix_d = handle;
#endif
  nhdp = NaClHostDescPosixMake(posix_d, NACL_ABI_O_RDWR);
  if (NULL == nhdp)
    return NULL;
  desc = NaClDescIoDescMake(nhdp);
  return &desc->base;
}

struct NaClDescIoDesc *NaClDescIoDescOpen(char  *path,
                                          int   mode,
                                          int   perms) {
  struct NaClHostDesc *nhdp;

  nhdp = malloc(sizeof *nhdp);
  if (NULL == nhdp) {
    NaClLog(LOG_FATAL, "NaClDescIoDescOpen: no memory for %s\n", path);
  }
  if (0 != NaClHostDescOpen(nhdp, path, mode, perms)) {
    NaClLog(LOG_FATAL, "NaClDescIoDescOpen: NaClHostDescOpen failed for %s\n",
            path);
  }
  return NaClDescIoDescMake(nhdp);
}

static uintptr_t NaClDescIoDescMap(struct NaClDesc         *vself,
                                   struct NaClDescEffector *effp,
                                   void                    *start_addr,
                                   size_t                  len,
                                   int                     prot,
                                   int                     flags,
                                   nacl_off64_t            offset) {
  struct NaClDescIoDesc *self = (struct NaClDescIoDesc *) vself;
  uintptr_t             status;
  uintptr_t             addr;

  /*
   * prot must be PROT_NONE or a combination of PROT_{READ|WRITE}
   */
  if (0 != (~(NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE) & prot)) {
    NaClLog(LOG_INFO,
            ("NaClDescIoDescMap: prot has other bits"
             " than PROT_{READ|WRITE}\n"));
    return -NACL_ABI_EINVAL;
  }

  if (0 == (NACL_ABI_MAP_FIXED & flags) && NULL == start_addr) {
    NaClLog(LOG_INFO,
            ("NaClDescIoDescMap: Mapping not NACL_ABI_MAP_FIXED"
             " but start_addr is NULL\n"));
  }

  if (0 == (NACL_ABI_MAP_FIXED & flags)) {
    if (!NaClFindAddressSpace(&addr, len)) {
      NaClLog(1, "NaClDescIoDescMap: no address space?\n");
      return -NACL_ABI_ENOMEM;
    }
    start_addr = (void *) addr;
  }
  flags |= NACL_ABI_MAP_FIXED;

  status = NaClHostDescMap((NULL == self) ? NULL : self->hd,
                           effp,
                           (void *) start_addr,
                           len,
                           prot,
                           flags,
                           offset);
  if (NACL_MAP_FAILED == (void *) status) {
    return -NACL_ABI_E_MOVE_ADDRESS_SPACE;
  }
  return (uintptr_t) start_addr;
}

uintptr_t NaClDescIoDescMapAnon(struct NaClDescEffector *effp,
                                void                    *start_addr,
                                size_t                  len,
                                int                     prot,
                                int                     flags,
                                nacl_off64_t            offset) {
  return NaClDescIoDescMap((struct NaClDesc *) NULL, effp, start_addr, len,
                           prot, flags, offset);
}

#if NACL_WINDOWS
static int NaClDescIoDescUnmapUnsafe(struct NaClDesc  *vself,
                                     void             *start_addr,
                                     size_t           len) {
  UNREFERENCED_PARAMETER(vself);
  return NaClHostDescUnmapUnsafe(start_addr, len);
}
#endif

static ssize_t NaClDescIoDescRead(struct NaClDesc          *vself,
                                  void                     *buf,
                                  size_t                   len) {
  struct NaClDescIoDesc *self = (struct NaClDescIoDesc *) vself;

  return NaClHostDescRead(self->hd, buf, len);
}

static ssize_t NaClDescIoDescWrite(struct NaClDesc         *vself,
                                   void const              *buf,
                                   size_t                  len) {
  struct NaClDescIoDesc *self = (struct NaClDescIoDesc *) vself;

  return NaClHostDescWrite(self->hd, buf, len);
}

static nacl_off64_t NaClDescIoDescSeek(struct NaClDesc          *vself,
                                       nacl_off64_t             offset,
                                       int                      whence) {
  struct NaClDescIoDesc *self = (struct NaClDescIoDesc *) vself;

  return NaClHostDescSeek(self->hd, offset, whence);
}

static int NaClDescIoDescIoctl(struct NaClDesc         *vself,
                               int                     request,
                               void                    *arg) {
  struct NaClDescIoDesc *self = (struct NaClDescIoDesc *) vself;

  return NaClHostDescIoctl(self->hd, request, arg);
}

static int NaClDescIoDescFstat(struct NaClDesc         *vself,
                               struct nacl_abi_stat    *statbuf) {
  struct NaClDescIoDesc *self = (struct NaClDescIoDesc *) vself;
  int                   rv;
  nacl_host_stat_t      hstatbuf;

  rv = NaClHostDescFstat(self->hd, &hstatbuf);
  if (0 != rv) {
    return rv;
  }
  return NaClAbiStatHostDescStatXlateCtor(statbuf, &hstatbuf);
}

static int NaClDescIoDescExternalizeSize(struct NaClDesc *vself,
                                         size_t          *nbytes,
                                         size_t          *nhandles) {
  int rv;

  rv = NaClDescExternalizeSize(vself, nbytes, nhandles);
  if (0 != rv) {
    return rv;
  }
  *nhandles += 1;
  return 0;
}

static int NaClDescIoDescExternalize(struct NaClDesc           *vself,
                                     struct NaClDescXferState  *xfer) {
  struct NaClDescIoDesc *self = (struct NaClDescIoDesc *) vself;
  int rv;
#if NACL_WINDOWS
  HANDLE  h;
#endif

  rv = NaClDescExternalize(vself, xfer);
  if (0 != rv) {
    return rv;
  }

#if NACL_WINDOWS
  h = (HANDLE) _get_osfhandle(self->hd->d);

  *xfer->next_handle++ = (NaClHandle) h;
#else
  *xfer->next_handle++ = self->hd->d;
#endif
  return 0;
}

static struct NaClDescVtbl const kNaClDescIoDescVtbl = {
  {
    NaClDescIoDescDtor,
  },
  NaClDescIoDescMap,
#if NACL_WINDOWS
  NaClDescIoDescUnmapUnsafe,
#else
  NACL_DESC_UNMAP_NOT_IMPLEMENTED
#endif
  NaClDescIoDescRead,
  NaClDescIoDescWrite,
  NaClDescIoDescSeek,
  NaClDescIoDescIoctl,
  NaClDescIoDescFstat,
  NaClDescGetdentsNotImplemented,
  NaClDescIoDescExternalizeSize,
  NaClDescIoDescExternalize,
  NaClDescLockNotImplemented,
  NaClDescTryLockNotImplemented,
  NaClDescUnlockNotImplemented,
  NaClDescWaitNotImplemented,
  NaClDescTimedWaitAbsNotImplemented,
  NaClDescSignalNotImplemented,
  NaClDescBroadcastNotImplemented,
  NaClDescSendMsgNotImplemented,
  NaClDescRecvMsgNotImplemented,
  NaClDescLowLevelSendMsgNotImplemented,
  NaClDescLowLevelRecvMsgNotImplemented,
  NaClDescConnectAddrNotImplemented,
  NaClDescAcceptConnNotImplemented,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
  NaClDescSetMetadata,
  NaClDescGetMetadata,
  NaClDescSetFlags,
  NaClDescGetFlags,
  NACL_DESC_HOST_IO,
};

/* set *out_desc to struct NaClDescIo * output */
int NaClDescIoInternalize(struct NaClDesc               **out_desc,
                          struct NaClDescXferState      *xfer,
                          struct NaClDescQuotaInterface *quota_interface) {
  int                   rv;
  NaClHandle            h;
  int                   d;
  struct NaClHostDesc   *nhdp;
  struct NaClDescIoDesc *ndidp;

  UNREFERENCED_PARAMETER(quota_interface);
  rv = -NACL_ABI_EIO;  /* catch-all */
  h = NACL_INVALID_HANDLE;
  nhdp = NULL;
  ndidp = NULL;

  nhdp = malloc(sizeof *nhdp);
  if (NULL == nhdp) {
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  ndidp = malloc(sizeof *ndidp);
  if (!ndidp) {
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  if (!NaClDescInternalizeCtor((struct NaClDesc *) ndidp, xfer)) {
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  if (xfer->next_handle == xfer->handle_buffer_end) {
    rv = -NACL_ABI_EIO;
    goto cleanup_ndidp_dtor;
  }
  h = *xfer->next_handle;
  *xfer->next_handle++ = NACL_INVALID_HANDLE;
#if NACL_WINDOWS
  if (-1 == (d = _open_osfhandle((intptr_t) h, _O_RDWR | _O_BINARY))) {
    rv = -NACL_ABI_EIO;
    goto cleanup_ndidp_dtor;
  }
#else
  d = h;
#endif
  /*
   * We mark it as read/write, but don't really know for sure until we
   * try to make those syscalls (in which case we'd get EBADF).
   */
  if ((rv = NaClHostDescPosixTake(nhdp, d, NACL_ABI_O_RDWR)) < 0) {
    goto cleanup_ndidp_dtor;
  }
  h = NACL_INVALID_HANDLE;  /* nhdp took ownership of h */

  if (!NaClDescIoDescSubclassCtor(ndidp, nhdp)) {
    rv = -NACL_ABI_ENOMEM;
    goto cleanup_nhdp_dtor;
  }
  /*
   * ndidp took ownership of nhdp, now give ownership of ndidp to caller.
   */
  *out_desc = (struct NaClDesc *) ndidp;
  rv = 0;
 cleanup_nhdp_dtor:
  if (rv < 0) {
    (void) NaClHostDescClose(nhdp);
  }
 cleanup_ndidp_dtor:
  if (rv < 0) {
    NaClDescSafeUnref((struct NaClDesc *) ndidp);
    ndidp = NULL;
  }
 cleanup:
  if (rv < 0) {
    free(nhdp);
    free(ndidp);
    if (NACL_INVALID_HANDLE != h) {
      (void) NaClClose(h);
    }
  }
  return rv;
}
