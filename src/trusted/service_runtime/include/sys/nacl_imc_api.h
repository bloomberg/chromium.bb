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

/*
 * NaCl Service Runtime.  IMC API.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_SYS_NACL_IMC_API_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_SYS_NACL_IMC_API_H_

/*
 * This file defines the C API for NativeClient applications.  The
 * ABI is implicitly defined.
 */

#include "native_client/src/trusted/service_runtime/include/bits/nacl_imc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TODO(sehr): there should be one instance to avoid conflicting definitions.
 */
#ifndef __nacl_handle_defined
#define __nacl_handle_defined
#if NACL_WINDOWS
typedef HANDLE NaClHandle;
#else
typedef int NaClHandle;
#endif
#endif

struct NaClImcMsgIoVec {
  void    *base;
  size_t  length;
};

struct NaClImcMsgHdr {
  struct NaClImcMsgIoVec  *iov;
  size_t                  iov_length;
  int                     *descv;
  size_t                  desc_length;
  int                     flags;
};

/*
 * NACL_ABI_IMC_IOVEC_MAX: How many struct NaClIOVec are permitted?
 * These are copied to kernel space in order to translate/validate
 * addresses, and are on the thread stack when processing
 * NaClSysSendmsg and NaClSysRecvmsg syscalls.  Each object takes 8
 * bytes, so beware running into NACL_KERN_STACK_SIZE above.
 */
#define NACL_ABI_IMC_IOVEC_MAX      256

/*
 * NAC_ABI_IMC_DESC_MAX: How many descriptors are permitted?  Each
 * object is 4 bytes.  An array of ints are on the kernel stack.
 *
 * TODO(bsy): coordinate w/ NACL_HANDLE_COUNT_MAX in nacl_imc_c.h.
 * Current IMC-imposed limit seems way too small.
 */
#define NACL_ABI_IMC_USER_DESC_MAX  8
#define NACL_ABI_IMC_DESC_MAX       8

/*
 * NACL_ABI_IMC_USER_BYTES_MAX: read must go into a kernel buffer first
 * before a variable-length header describing the number and types of
 * NaClHandles encoded is parsed, with the rest of the data not yet
 * consumed turning into user data.
 */
#define NACL_ABI_IMC_USER_BYTES_MAX    (128 << 10)
#define NACL_ABI_IMC_BYTES_MAX                   \
  (NACL_ABI_IMC_USER_BYTES_MAX                   \
   + (1 + NACL_PATH_MAX) * NACL_ABI_IMC_USER_DESC_MAX + 16)
/*
 * 4096 + (1 + 28) * 256 = 11520, so the read buffer must be malloc'd
 * or be part of the NaClAppThread structure; the kernel thread stack
 * is too small for it.
 *
 * NB: the header has an end tag and the size is rounded up to the
 * next 16 bytes.
 */

#ifdef __cplusplus
}
#endif

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_SYS_NACL_IMC_API_H_ */
