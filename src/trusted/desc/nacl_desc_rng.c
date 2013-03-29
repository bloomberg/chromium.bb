/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * A NaClDesc subclass that exposes the platform secure RNG
 * implementation.
 */

#include <string.h>

#include "native_client/src/trusted/desc/nacl_desc_rng.h"

#include "native_client/src/shared/platform/nacl_secure_random.h"
#include "native_client/src/shared/platform/nacl_secure_random_base.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"

#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

static struct NaClDescVtbl const kNaClDescRngVtbl;  /* fwd */

static int NaClDescRngSubclassCtor(struct NaClDescRng  *self) {
  if (!NaClSecureRngCtor(&self->rng)) {
    goto rng_ctor_fail;
  }
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClDescRngVtbl;
  return 1;

  /* failure cleanup */
 rng_ctor_fail:
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
  return 0;
}

int NaClDescRngCtor(struct NaClDescRng  *self) {
  int rv;
  if (!NaClDescCtor((struct NaClDesc *) self)) {
    return 0;
  }
  rv = NaClDescRngSubclassCtor(self);
  if (!rv) {
    (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
  }
  return rv;
}

static void NaClDescRngDtor(struct NaClRefCount *vself) {
  struct NaClDescRng *self = (struct NaClDescRng *) vself;

  (*NACL_VTBL(NaClSecureRngIf, &self->rng)->Dtor)(
      (struct NaClSecureRngIf *) &self->rng);
  NACL_VTBL(NaClDesc, self) = &kNaClDescVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
}

static ssize_t NaClDescRngRead(struct NaClDesc  *vself,
                               void             *buf,
                               size_t           len) {
  struct NaClDescRng *self = (struct NaClDescRng *) vself;

  (*NACL_VTBL(NaClSecureRngIf, &self->rng)->GenBytes)(
      (struct NaClSecureRngIf *) &self->rng, buf, len);
  return len;
}

static ssize_t NaClDescRngWrite(struct NaClDesc *vself,
                                void const      *buf,
                                size_t          len) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(buf);

  /*
   * Eventually we may want to have secure pseudorandom number
   * generators that permit mixing user-supplied data -- presumably
   * low entropy, from timing of events or something like that -- into
   * the generator state.  This must be done carefully, of course,
   * since we would not want the user-supplied data to destroy the
   * internal generator's entropy.
   */
  return len;
}

static int NaClDescRngFstat(struct NaClDesc       *vself,
                            struct nacl_abi_stat  *statbuf) {
  UNREFERENCED_PARAMETER(vself);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_dev = 0;
#if defined(NACL_MASK_INODES)
  statbuf->nacl_abi_st_ino = NACL_FAKE_INODE_NUM;
#else
  statbuf->nacl_abi_st_ino = 0;
#endif
  statbuf->nacl_abi_st_mode = NACL_ABI_S_IRUSR | NACL_ABI_S_IFCHR;
  statbuf->nacl_abi_st_nlink = 1;
  statbuf->nacl_abi_st_uid = -1;
  statbuf->nacl_abi_st_gid = -1;
  statbuf->nacl_abi_st_rdev = 0;
  statbuf->nacl_abi_st_size = 0;
  statbuf->nacl_abi_st_blksize = 0;
  statbuf->nacl_abi_st_blocks = 0;
  statbuf->nacl_abi_st_atime = 0;
  statbuf->nacl_abi_st_atimensec = 0;
  statbuf->nacl_abi_st_mtime = 0;
  statbuf->nacl_abi_st_mtimensec = 0;
  statbuf->nacl_abi_st_ctime = 0;
  statbuf->nacl_abi_st_ctimensec = 0;

  return 0;
}

/*
 * We allow descriptor "transfer", where in reality we create a
 * separate rng locally at the recipient end.  This is arguably
 * semantically different since there is no shared access to the same
 * generator; on the other hand, it should be polynomial-time
 * indistinguishable since the output is supposed to be
 * cryptographically secure.
 */
static int NaClDescRngExternalizeSize(struct NaClDesc *vself,
                                      size_t          *nbytes,
                                      size_t          *nhandles) {
  return NaClDescExternalizeSize(vself, nbytes, nhandles);
}

static int NaClDescRngExternalize(struct NaClDesc           *vself,
                                  struct NaClDescXferState  *xfer) {
  return NaClDescExternalize(vself, xfer);
}

static struct NaClDescVtbl const kNaClDescRngVtbl = {
  {
    NaClDescRngDtor,
  },
  NaClDescMapNotImplemented,
  NACL_DESC_UNMAP_NOT_IMPLEMENTED
  NaClDescRngRead,
  NaClDescRngWrite,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescRngFstat,
  NaClDescGetdentsNotImplemented,
  NaClDescRngExternalizeSize,
  NaClDescRngExternalize,
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
  NACL_DESC_DEVICE_RNG,
};

int NaClDescRngInternalize(struct NaClDesc               **out_desc,
                           struct NaClDescXferState      *xfer,
                           struct NaClDescQuotaInterface *quota_interface) {
  int                 rv;
  struct NaClDescRng  *rng = malloc(sizeof *rng);

  UNREFERENCED_PARAMETER(xfer);
  UNREFERENCED_PARAMETER(quota_interface);
  if (NULL == rng) {
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  if (!NaClDescInternalizeCtor((struct NaClDesc *) rng, xfer)) {
    free(rng);
    rng = NULL;
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  if (!NaClDescRngSubclassCtor(rng)) {
    rv = -NACL_ABI_EIO;
    goto cleanup;
  }
  *out_desc = (struct NaClDesc *) rng;
  rv = 0;  /* yay! */
 cleanup:
  if (rv < 0) {
    NaClDescSafeUnref((struct NaClDesc *) rng);
  }
  return rv;
}
