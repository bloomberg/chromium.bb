/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
 * SRPC_DEBUG enables trace output printing.
 */
#define SRPC_DEBUG
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
 * We have to do this for now, because portability.h doesn't work for
 * Native Client compilations.
 * TODO(sehr): make portability.h work for Native Client compilations.
 */
#if NACL_WINDOWS
# define UNREFERENCED_PARAMETER(P) (P)
#else
# define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif

/*
 * Wrappers for accesses to read and write via the IMC layer.
 */
extern void __NaClSrpcImcBufferCtor(NaClSrpcImcBuffer* buffer,
                                    int is_write_buf);

extern NaClSrpcImcBuffer* __NaClSrpcImcFillbuf(NaClSrpcChannel* channel);

extern nacl_abi_size_t __NaClSrpcImcRead(NaClSrpcImcBuffer* buffer,
                                         nacl_abi_size_t elt_size,
                                         nacl_abi_size_t n_elt,
                                         void* target);

extern void __NaClSrpcImcRefill(NaClSrpcImcBuffer* buffer);

extern nacl_abi_size_t __NaClSrpcImcWrite(const void* source,
                                          nacl_abi_size_t elt_size,
                                          nacl_abi_size_t n_elt,
                                          NaClSrpcImcBuffer* buffer);

extern int __NaClSrpcImcFlush(NaClSrpcImcBuffer* buffer,
                              NaClSrpcChannel* channel);

/* Descriptor passing */
extern NaClSrpcImcDescType __NaClSrpcImcReadDesc(NaClSrpcImcBuffer* buffer);
extern int __NaClSrpcImcWriteDesc(NaClSrpcImcDescType desc,
                                  NaClSrpcImcBuffer* buffer);
/*
 * Maximum sendmsg buffer size.
 */
extern nacl_abi_size_t kNaClSrpcMaxImcSendmsgSize;


EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_SHARED_SRPC_NACL_SRPC_INTERNAL_H_ */
