/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Subclass of NaClDesc which passes write output data to the browser
 * using the reverse channel, to (eventually) show up as PostMessage
 * data.
 *
 * A NaClDescPostMessage object pretends to be a character device, so
 * that I/O packages that fstat to determine buffering strategy will
 * work correctly.  The only other syscall that it implements is
 * write, and the data is sent through the reverse channel interface
 * to the browser's JavaScript environment.
 */

#include <string.h>

#include "native_client/src/trusted/service_runtime/nacl_desc_postmessage.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


static struct NaClDescVtbl const kNaClDescPostMessageVtbl;  /* fwd */

int NaClDescPostMessageCtor(struct NaClDescPostMessage  *self,
                            struct NaClApp              *nap) {
  NaClLog(4, "Entered NaClDescPostMessageCtor\n");
  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl const *) NULL;
  if (!NaClDescCtor(&self->base)) {
    NaClLog(4, "Leaving NaClDescPostMessageCtor: failed\n");
    return 0;
  }
  self->nap = nap;
  self->error = 0;
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl const *) &kNaClDescPostMessageVtbl;
  NaClLog(4, " Write vfptr = %"NACL_PRIxPTR"\n",
          (uintptr_t) NACL_VTBL(NaClDesc, self)->Write);
  NaClLog(4, "Leaving NaClDescPostMessageCtor: success\n");
  return 1;
}

static void NaClDescPostMessageDtor(struct NaClRefCount *vself) {
  struct NaClDescPostMessage  *self = (struct NaClDescPostMessage *) vself;

  NaClLog(4, "Entered NaClDescPostMessageDtor\n");
  self->nap = NULL;
  NACL_VTBL(NaClRefCount, vself) =
      (struct NaClRefCountVtbl const *) &kNaClDescVtbl;
  (*NACL_VTBL(NaClRefCount, vself)->Dtor)(vself);
  NaClLog(4, "Leaving NaClDescPostMessageDtor\n");
}

static ssize_t NaClDescPostMessageWrite(struct NaClDesc *vself,
                                        void const      *buf,
                                        size_t          len) {
  struct NaClDescPostMessage  *self = (struct NaClDescPostMessage *) vself;
  NaClSrpcError               rpc_result;
  int                         num_written = 0;
  ssize_t                     rv = -NACL_ABI_EIO;

  NaClLog(4, "Entered NaClDescPostMessageWrite(..., %"NACL_PRIuS")\n", len);
  if (0 != self->error) {
    return self->error;
  }
  NaClXMutexLock(&self->nap->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED !=
      self->nap->reverse_channel_initialization_state) {
    NaClLog(LOG_FATAL,
            "NaClDescPostMessageWrite: Reverse channel not initialized\n");
  }
  if (len > NACL_ABI_SIZE_T_MAX) {
    len = NACL_ABI_SIZE_T_MAX;  /* fits in an int32_t */
  }
  rpc_result = NaClSrpcInvokeBySignature(&self->nap->reverse_channel,
                                         NACL_REVERSE_CONTROL_POST_MESSAGE,
                                         len,
                                         buf,
                                         &num_written);
  if (NACL_SRPC_RESULT_OK != rpc_result || num_written > (int) len) {
    /*
     * A conforming interface implementation could return an errno,
     * but should never return a larger value.
     */
    rv = -NACL_ABI_EIO;
    /*
     * make this error permanent; other negative errno returns are
     * considered transient.
     */
    self->error = rv;
  } else {
    rv = (ssize_t) num_written;
  }
  NaClXMutexUnlock(&self->nap->mu);
  NaClLog(4, "Leaving NaClDescPostMessageWrite (%"NACL_PRIuS")\n", rv);
  return rv;
}

static int NaClDescPostMessageFstat(struct NaClDesc       *vself,
                                    struct nacl_abi_stat  *statbuf) {
  UNREFERENCED_PARAMETER(vself);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_ino = NACL_FAKE_INODE_NUM;
  statbuf->nacl_abi_st_mode = (NACL_ABI_S_IFCHR | NACL_ABI_S_IWUSR);
  statbuf->nacl_abi_st_nlink = 1;
  statbuf->nacl_abi_st_uid = -1;
  statbuf->nacl_abi_st_gid = -1;
  return 0;
}

static struct NaClDescVtbl const kNaClDescPostMessageVtbl = {
  {
    NaClDescPostMessageDtor,
  },
  NaClDescMapNotImplemented,
  NACL_DESC_UNMAP_NOT_IMPLEMENTED
  NaClDescReadNotImplemented,
  NaClDescPostMessageWrite,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescPostMessageFstat,
  NaClDescGetdentsNotImplemented,
  NaClDescExternalizeSizeNotImplemented,
  NaClDescExternalizeNotImplemented,
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
  NACL_DESC_DEVICE_POSTMESSAGE,
};
