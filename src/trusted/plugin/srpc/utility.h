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
#if SRPC_PLUGIN_DEBUG
#  define dprintf(args) do { \
    printf("%08"PRIx32":", NaClThreadId());    \
    printf args; \
    fflush(stdout); \
   } while(0)
#else
#  define dprintf(args) do { if (0) { printf args; } } while (0)
/* allows DCE but compiler can still do format string checks */
#endif

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_UTILITY_H_
