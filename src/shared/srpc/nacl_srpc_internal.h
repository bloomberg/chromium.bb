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


#ifndef NATIVE_CLIENT_SRC_SHARED_SRPC_NACL_SRPC_INTERNAL_H_
#define NATIVE_CLIENT_SRC_SHARED_SRPC_NACL_SRPC_INTERNAL_H_

/*
 * Avoid emacs' penchant for auto-indenting extern "C" blocks.
 */
#ifndef EXTERN_C_BEGIN
#  ifdef __cplusplus
#    define EXTERN_C_BEGIN extern "C" {
#    define EXTERN_C_END   }
#  else
#    define EXTERN_C_BEGIN
#    define EXTERN_C_END
#  endif  /* __cplusplus */
#endif

EXTERN_C_BEGIN

#ifdef __native_client__
#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>
typedef int SRPC_IMC_DESC_TYPE;
#define SRPC_DESC_MAX    IMC_USER_DESC_MAX
#define SIDE "NC:   "
#else
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
typedef struct NaClDesc* SRPC_IMC_DESC_TYPE;
#define SIDE "HOST: "
#define SRPC_DESC_MAX    NACL_ABI_IMC_USER_DESC_MAX
#endif

/*
#define SRPC_DEBUG
 * SRPC_DEBUG enables trace output printing.
 */
extern int gNaClSrpcDebugPrintEnabled;
extern int __NaClSrpcDebugPrintCheckEnv();

#ifdef SRPC_DEBUG
#define dprintf(args) do {                                              \
    if (-1 == gNaClSrpcDebugPrintEnabled) {                             \
      gNaClSrpcDebugPrintEnabled = __NaClSrpcDebugPrintCheckEnv();      \
    }                                                                   \
    if (0 != gNaClSrpcDebugPrintEnabled) {                              \
      printf args;                                                      \
      fflush(stdout);                                                   \
    }                                                                   \
  } while (0)
#else
#define dprintf(args) do { \
    if (0) {               \
      printf args;         \
    }                      \
  } while (0)
#endif

/*
 * Wrappers for accesses to read and write via the IMC layer.
 */
extern void __NaClSrpcImcBufferCtor(NaClSrpcImcBuffer* buffer,
                                    int is_write_buf);

extern NaClSrpcImcBuffer* __NaClSrpcImcFillbuf(NaClSrpcChannel* channel);

extern int __NaClSrpcImcRead(NaClSrpcImcBuffer* buffer,
                             size_t elt_size,
                             size_t n_elt,
                             void* target);

extern void __NaClSrpcImcRefill(NaClSrpcImcBuffer* buffer);

extern int __NaClSrpcImcWrite(const void* source,
                              size_t elt_size,
                              size_t n_elt,
                              NaClSrpcImcBuffer* buffer);

extern int __NaClSrpcImcFlush(NaClSrpcImcBuffer* buffer,
                              NaClSrpcChannel* channel);

/* Descriptor passing */
extern NaClSrpcImcDescType __NaClSrpcImcReadDesc(NaClSrpcImcBuffer* buffer);
extern int __NaClSrpcImcWriteDesc(NaClSrpcImcDescType desc,
                                  NaClSrpcImcBuffer* buffer);
/*
 * Utility functions.
 */
extern double __NaClSrpcGetUsec();

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_SHARED_SRPC_NACL_SRPC_INTERNAL_H_ */
