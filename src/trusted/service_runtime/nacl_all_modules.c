/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Gather ye all module initializations and finalizations here.
 */
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/debug_stub/debug_stub.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/handle_pass/ldr_handle.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_thread_nice.h"
#include "native_client/src/trusted/service_runtime/nacl_tls.h"

// #define NACL_DEBUG_STUB 1

void  NaClAllModulesInit(void) {
#ifdef NACL_DEBUG_STUB
  NaClDebugStubInit();
#endif
  NaClNrdAllModulesInit();
  NaClGlobalModuleInit();  /* various global variables */
  NaClSrpcModuleInit();
  NaClTlsInit();
  NaClSyscallTableInit();
  NaClThreadNiceInit();
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassLdrInit();
#endif
  NaClSignalHandlerInit();
}


void NaClAllModulesFini(void) {
  NaClSignalHandlerFini();
  NaClTlsFini();
  NaClSrpcModuleFini();
  NaClGlobalModuleFini();
  NaClNrdAllModulesFini();
#ifdef NACL_DEBUG_STUB
  NaClDebugStubFini();
#endif
}
