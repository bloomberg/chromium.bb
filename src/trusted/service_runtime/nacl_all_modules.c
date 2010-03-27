/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Gather ye all module initializations and finalizations here.
 */
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/handle_pass/ldr_handle.h"
#include "native_client/src/trusted/nacl_breakpad/nacl_breakpad.h"
#include "native_client/src/trusted/service_runtime/nacl_audio_video.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_thread_nice.h"
#include "native_client/src/trusted/service_runtime/nacl_tls.h"


void  NaClAllModulesInit(void) {
#ifdef NACL_BREAKPAD
  NaClBreakpadInit();
#endif
  NaClNrdAllModulesInit();
  NaClGlobalModuleInit();  /* various global variables */
  NaClTlsInit();
#if defined(HAVE_SDL)
  NaClMultimediaModuleInit();
#endif
  NaClSyscallTableInit();
  NaClThreadNiceInit();
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassLdrInit();
#endif
}


void NaClAllModulesFini(void) {
#if defined(HAVE_SDL)
  NaClMultimediaModuleFini();
#endif
  NaClTlsFini();
  NaClGlobalModuleFini();
  NaClNrdAllModulesFini();
#ifdef NACL_BREAKPAD
  NaClBreakpadFini();
#endif
}
