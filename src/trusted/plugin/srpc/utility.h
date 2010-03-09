/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_UTILITY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_UTILITY_H_

#include <setjmp.h>
#include <signal.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_threads.h"

#define SRPC_PLUGIN_DEBUG 0

#ifndef SRPC_PLUGIN_DEBUG
#define SRPC_PLUGIN_DEBUG 1
#endif

namespace nacl_srpc {

// CatchSignals encapsulates the setup and removal of application-defined
// signal handlers.
// TODO(sehr): need to revisit thread-specific, etc., signal handling

class ScopedCatchSignals {
 public:
  typedef void (*SigHandlerType)(int signal_number);
  // Installs handler as the signal handler for SIGSEGV and SIGILL.
  explicit ScopedCatchSignals(SigHandlerType handler) {
    prev_segv_handler_ = signal(SIGSEGV, handler);
    prev_ill_handler_ = signal(SIGILL, handler);
#ifndef _MSC_VER
    prev_pipe_handler_ = signal(SIGPIPE, handler);
#endif
  }
  // Restores the previous signal handler
  ~ScopedCatchSignals() {
    signal(SIGSEGV, prev_segv_handler_);
    signal(SIGILL, prev_ill_handler_);
#ifndef _MSC_VER
    signal(SIGPIPE, prev_pipe_handler_);
#endif
  }
 private:
  SigHandlerType prev_segv_handler_;
  SigHandlerType prev_ill_handler_;
#ifndef _MSC_VER
  SigHandlerType prev_pipe_handler_;
#endif
};

// Platform abstraction for setjmp and longjmp
// TODO(sehr): move to portability.h
#ifdef _MSC_VER
#define PLUGIN_SETJMP(buf, mask)   setjmp(buf)
#define PLUGIN_LONGJMP(buf, value) longjmp(buf, value)
#define PLUGIN_JMPBUF jmp_buf
#else
#define PLUGIN_SETJMP(buf, mask)   sigsetjmp(buf, mask)
#define PLUGIN_LONGJMP(buf, value) siglongjmp(buf, value)
#define PLUGIN_JMPBUF sigjmp_buf
#endif  // _MSC_VER

// Debugging print utility
extern int gNaClPluginDebugPrintEnabled;
extern int NaClPluginDebugPrintCheckEnv();
#if SRPC_PLUGIN_DEBUG
#  define dprintf(args) do {                                          \
    if (-1 == ::nacl_srpc::gNaClPluginDebugPrintEnabled) {            \
      ::nacl_srpc::gNaClPluginDebugPrintEnabled =                     \
          ::nacl_srpc::NaClPluginDebugPrintCheckEnv();                \
    }                                                                 \
    if (0 != ::nacl_srpc::gNaClPluginDebugPrintEnabled) {             \
      printf("%08"NACL_PRIx32":", NaClThreadId());                    \
      printf args;                                                    \
      fflush(stdout);                                                 \
    }                                                                 \
  } while (0)
#else
#  define dprintf(args) do { if (0) { printf args; } } while (0)
/* allows DCE but compiler can still do format string checks */
#endif

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_UTILITY_H_
