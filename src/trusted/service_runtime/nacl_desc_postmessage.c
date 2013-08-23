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
#include "native_client/src/trusted/service_runtime/nacl_runtime_host_interface.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


static struct NaClDescVtbl const kNaClDescPostMessageVtbl;  /* fwd */

int NaClDescPostMessageCtor(struct NaClDescPostMessage      *self,
                            struct NaClRuntimeHostInterface *runtime_host) {
  NaClLog(4, "Entered NaClDescPostMessageCtor\n");
  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl const *) NULL;
  if (!NaClDescCtor(&self->base)) {
    NaClLog(4, "Leaving NaClDescPostMessageCtor: failed\n");
    return 0;
  }
  self->runtime_host = (struct NaClRuntimeHostInterface *)
      NaClRefCountRef((struct NaClRefCount *) runtime_host);
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

  NaClRefCountUnref((struct NaClRefCount *) self->runtime_host);

  NACL_VTBL(NaClRefCount, vself) =
      (struct NaClRefCountVtbl const *) &kNaClDescVtbl;
  (*NACL_VTBL(NaClRefCount, vself)->Dtor)(vself);
  NaClLog(4, "Leaving NaClDescPostMessageDtor\n");
}

static ssize_t NaClDescPostMessageWrite(struct NaClDesc *vself,
                                        void const      *buf,
                                        size_t          len) {
  struct NaClDescPostMessage  *self = (struct NaClDescPostMessage *) vself;
  ssize_t                     num_written = 0;

  NaClLog(4, "Entered NaClDescPostMessageWrite(..., %"NACL_PRIuS")\n", len);
  num_written = (*NACL_VTBL(NaClRuntimeHostInterface, self->runtime_host)->
                 PostMessage)(self->runtime_host, buf, len);
  NaClLog(4, "Leaving NaClDescPostMessageWrite:"
          " num_written %"NACL_PRIuS"\n", num_written);
  return num_written;
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
  NaClDescPReadNotImplemented,
  NaClDescPWriteNotImplemented,
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
