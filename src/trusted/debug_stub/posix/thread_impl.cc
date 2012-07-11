/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/atomic_ops.h"
#include "native_client/src/trusted/port/platform.h"
#include "native_client/src/trusted/port/thread.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

/*
 * Define the OS specific portions of IThread interface.
 */

namespace port {

static IThread::CatchFunc_t s_CatchFunc = NULL;
static void* s_CatchCookie = NULL;


static enum NaClSignalResult SignalHandler(int signal, void *ucontext) {
  struct NaClSignalContext context;
  NaClSignalContextFromHandler(&context, ucontext);
  if (NaClSignalContextIsUntrusted(&context)) {
    // Ensure signals are processed one at a time.
    // Spin if we are processing another signal already. This won't actually
    // spin for too long as that signal handler will suspend this thread.
    // TODO(eaeltsin): this is here because Target::Signal fails doing the same
    // using Target::sig_start_ IEvent. Investigate and remove this hack!
    static Atomic32 g_is_processing_signal = 0;
    while (CompareAndSwap(&g_is_processing_signal, 0, 1) != 0) {
      // Spin.
    }
    uint32_t thread_id = IPlatform::GetCurrentThread();
    IThread* thread = IThread::Acquire(thread_id);

    NaClSignalContextFromHandler(thread->GetContext(), ucontext);
    if (s_CatchFunc != NULL)
      s_CatchFunc(thread_id, signal, s_CatchCookie);
    NaClSignalContextToHandler(ucontext, thread->GetContext());

    IThread::Release(thread);
    // TODO(eaeltsin): remove this hack!
    while (CompareAndSwap(&g_is_processing_signal, 1, 0) != 1) {
      // Spin.
    }
    return NACL_SIGNAL_RETURN;
  } else {
    // Do not attempt to debug crashes in trusted code.
    return NACL_SIGNAL_SEARCH;
  }
}

void IThread::SetExceptionCatch(IThread::CatchFunc_t func, void *cookie) {
  NaClSignalHandlerAdd(SignalHandler);
  s_CatchFunc = func;
  s_CatchCookie = cookie;
}


}  // End of port namespace

