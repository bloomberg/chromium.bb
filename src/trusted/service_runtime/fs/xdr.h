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
 * NaCl file system mini XDR-like serialization of basic types.
 *
 * NB: this code has not been integrated with the rest of NaCl, and is
 * likely to change.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_FS_XDR_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_FS_XDR_H_

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"

EXTERN_C_BEGIN

/*
 * knowledge about sizes for types of various file system RPC message
 * structures' members is not baked in here, but only in
 * machine/_types.h
 *
 * memcpy is (typically) a compiler intrinsic, so we should get
 * efficient code while maintaining type safety.  there are no byte
 * order issues when IMC can only communicate between processes on the
 * system.
 *
 * we violate the convention for in/out arg order so that code for
 * externalizatin/internalization of structures can be written more
 * easily using macros.
 */

#define D(T)                                                            \
  static INLINE size_t nacl_ext_ ## T(char *buf, nacl_abi_ ## T datum)  \
  {                                                                     \
    memcpy((void *) buf, (void *) &datum, sizeof datum);                \
    return sizeof datum;                                                \
  }                                                                     \
  static INLINE size_t nacl_int_ ## T(char *buf, nacl_abi_ ## T *datum) \
  {                                                                     \
    memcpy((void *) datum, (void *) buf, sizeof datum);                 \
    return sizeof datum;                                                \
  }

D(dev_t)
D(ino_t)
D(mode_t)
D(uid_t)
D(gid_t)
D(nlink_t)
D(off_t)
D(time_t)
D(size_t)
D(ssize_t)

#undef D

#define D(T)                                                            \
  static INLINE size_t nacl_ext_ ## T(char *buf, T datum)               \
  {                                                                     \
    memcpy((void *) buf, (void *) &datum, sizeof datum);                \
    return sizeof datum;                                                \
  }                                                                     \
  static INLINE size_t nacl_int_ ## T(char *buf, T *datum)              \
  {                                                                     \
    memcpy((void *) datum, (void *) buf, sizeof datum);                 \
    return sizeof datum;                                                \
  }

D(int8_t)
D(int16_t)
D(int32_t)
D(int64_t)
D(uint8_t)
D(uint16_t)
D(uint32_t)
D(uint64_t)

#undef D

EXTERN_C_END

#endif /* NATIVE_CLIENT_SERVICE_RUNTIME_FS_XDR_H_ */
