/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NaCl Handle Transfer Protocol

#include "native_client/src/shared/imc/nacl_htp.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <limits>
/* TODO: eliminate this */
#if NACL_WINDOWS
#include <io.h>
#include <fcntl.h>
#endif

#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"


namespace nacl {

#ifndef __native_client__

int SendDatagram(HtpHandle socket, const HtpHeader* message, int flags) {
  ssize_t result = NaClImcSendTypedMessage(socket, NULL,
      reinterpret_cast<const struct NaClImcTypedMsgHdr*>(message), flags);

  if (result > std::numeric_limits<int>::max()) {
    result = -NACL_ABI_EOVERFLOW;
  }

  if (NaClIsNegErrno(result)) {
    // TODO(ilewis,bsy): should we be setting errno here?
    return -1;
  }

  // cast is safe due to checks above
  return static_cast<int>(result);
}

int ReceiveDatagram(HtpHandle socket, HtpHeader* message, int flags) {
  ssize_t result = NaClImcRecvTypedMessage(socket, NULL,
      reinterpret_cast<struct NaClImcTypedMsgHdr*>(message), flags);

  if (result > std::numeric_limits<int>::max()) {
    result = -NACL_ABI_EOVERFLOW;
  }

  if (NaClIsNegErrno(result)) {
    // TODO(ilewis,bsy): should we be setting errno here?
    return -1;
  }

  // cast is safe due to checks above
  return static_cast<int>(result);
}

int Close(HtpHandle handle) {
  if (handle) {
    NaClDescUnref(handle);
  }
  return 0;
}

void* Map(void* start, size_t length, int prot, int flags,
          HtpHandle memory, off_t offset) {
  NaClDescImcShm* desc = reinterpret_cast<NaClDescImcShm*>(memory);
  return Map(start, length, prot, flags, desc->h, offset);
}

HtpHandle CreateIoDesc(Handle handle) {
  if (handle == kInvalidHandle) {
    return NULL;
  }
  NaClHostDesc* nhdp = static_cast<NaClHostDesc*>(malloc(sizeof(*nhdp)));
  if (!nhdp) {
    return NULL;
  };
  NaClDescIoDesc* ndidp = static_cast<NaClDescIoDesc*>(malloc(sizeof(*ndidp)));
  if (!ndidp) {
    free(nhdp);
    return NULL;
  }
  int fd;
#if NACL_WINDOWS
  fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                       _O_RDWR | _O_BINARY);
  if (fd == -1) {
    free(ndidp);
    free(nhdp);
    return NULL;
  }
#else
  fd = handle;
#endif
  // We mark it as read/write, but don't really know for sure until we
  // try to make those syscalls (in which case we'd get EBADF).
  if (0 <= NaClHostDescPosixTake(nhdp, fd, NACL_ABI_O_RDWR) &&
      NaClDescIoDescCtor(ndidp, nhdp)) {
    return reinterpret_cast<NaClDesc*>(ndidp);
  }
#if NACL_WINDOWS
  close(fd);
#endif
  free(ndidp);
  free(nhdp);
  return NULL;
}

HtpHandle CreateShmDesc(Handle handle, off_t length) {
  if (handle == kInvalidHandle) {
    return NULL;
  }
  NaClDescImcShm* desc = static_cast<NaClDescImcShm*>(malloc(sizeof(*desc)));
  if (desc == NULL) {
    return NULL;
  }
  if (!NaClDescImcShmCtor(desc, handle, length)) {
    free(desc);
    return NULL;
  }
  return reinterpret_cast<NaClDesc*>(desc);
}

HtpHandle CreateImcDesc(Handle handle) {
  if (handle == kInvalidHandle) {
    return NULL;
  }
  NaClDescImcDesc* desc = static_cast<NaClDescImcDesc*>(malloc(sizeof(*desc)));
  if (desc == NULL) {
    return NULL;
  }
  if (!NaClDescImcDescCtor(desc, handle)) {
    free(desc);
    return NULL;
  }
  return reinterpret_cast<NaClDesc*>(desc);
}

nacl_abi_ssize_t Read(HtpHandle handle, void* buffer, nacl_abi_size_t count) {
  // We're only actually capable of returning a signed size, so make
  // sure the request doesn't exceed that limit. Read() is allowed to return
  // a smaller value than "count" without setting errno, so we'll just
  // truncate here rather than raising an error.
  size_t realCount = nacl_abi_ssize_t_saturate(count);

  // Do the read.
  ssize_t result = NaClDescIoDescRead(handle,
                                      NULL,
                                      buffer,
                                      realCount);

  // This next conditional does a sanity check on the result. It is only
  // necessary on systems where SIZE_T and NACL_ABI_SIZE_T are different
  // data types.
#if !NACL_ABI_WORDSIZE_IS_NATIVE
  // Since the "count" parameter is a nacl_abi_size_t, and we already truncated
  // it to a nacl_abi_ssize_t, it would be horribly unusual if the return value
  // exceeded NACL_ABI_SIZE_T_MAX. Almost certainly it would indicate a fatal
  // error. So panic.
  if (result > NACL_ABI_SSIZE_T_MAX) {
    NaClLog(LOG_FATAL, "Overflow in return from NaClDescIoDescRead()\n");
  }
#endif

  return nacl_abi_ssize_t_saturate(result);
}

nacl_abi_ssize_t Write(HtpHandle handle,
                       const void* buffer,
                       nacl_abi_size_t count) {
  // We're only actually capable of returning a signed size, so make
  // sure the request doesn't exceed that limit. Write() is allowed to return
  // a smaller value than "count" without setting errno, so we'll just
  // truncate here rather than raising an error.
  size_t realCount = nacl_abi_ssize_t_saturate(count);

  // Do the read.
  ssize_t result = NaClDescIoDescWrite(handle,
                                       NULL,
                                       buffer,
                                       realCount);

  // This next conditional does a sanity check on the result. It is only
  // necessary on systems where SIZE_T and NACL_ABI_SIZE_T are different
  // data types.
#if (!NACL_ABI_WORDSIZE_IS_NATIVE)
  // Since the "count" parameter is a nacl_abi_size_t, and we already truncated
  // it to a nacl_abi_ssize_t, it would be horribly unusual if the return value
  // exceeded NACL_ABI_SIZE_T_MAX. Almost certainly it would indicate a fatal
  // error. So panic.
  if (result > NACL_ABI_SSIZE_T_MAX) {
    NaClLog(LOG_FATAL, "Overflow in return from NaClDescIoDescWrite()\n");
  }
#endif

  return nacl_abi_ssize_t_saturate(result);
}

#endif  // __native_client__

}  // namespace nacl
