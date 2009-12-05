/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// NaCl Handle Transfer Protocol

#include "native_client/src/shared/imc/nacl_htp.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
/* TODO: eliminate this */
#if NACL_WINDOWS
#include <io.h>
#include <fcntl.h>
#endif

#include "native_client/src/shared/platform/nacl_host_desc.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"

#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"


namespace nacl {

#ifndef __native_client__

int SendDatagram(HtpHandle socket, const HtpHeader* message, int flags) {
  int32_t result = NaClImcSendTypedMessage(socket, NULL,
      reinterpret_cast<const struct NaClImcTypedMsgHdr*>(message), flags);
  if (result < 0) {
    return -1;
  }
  return result;
}

int ReceiveDatagram(HtpHandle socket, HtpHeader* message, int flags) {
  int32_t result = NaClImcRecvTypedMessage(socket, NULL,
      reinterpret_cast<struct NaClImcTypedMsgHdr*>(message), flags);
  if (result < 0) {
    return -1;
  }
  return result;
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

int Read(HtpHandle handle, void* buffer, size_t count) {
  return NaClDescIoDescRead(handle, NULL, buffer, count);
}

int Write(HtpHandle handle, const void* buffer, size_t count) {
  return NaClDescIoDescWrite(handle, NULL, buffer, count);
}

#endif  // __native_client__

}  // namespace nacl
