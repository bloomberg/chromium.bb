/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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
    uint32_t thread_id = IPlatform::GetCurrentThread();
    IThread* thread = IThread::Acquire(thread_id);

    NaClSignalContextFromHandler(thread->GetContext(), ucontext);
    if (s_CatchFunc != NULL)
      s_CatchFunc(thread_id, signal, s_CatchCookie);
    NaClSignalContextToHandler(ucontext, thread->GetContext());

    IThread::Release(thread);
    return NACL_SIGNAL_RETURN;
  } else {
    // Do not attempt to debug crashes in trusted code.
    return NACL_SIGNAL_SEARCH;
  }
}

void IThread::SuspendOneThread(struct NaClAppThread *,
                               struct NaClSignalContext *) {
  // TODO(eaeltsin): implement this.
}

void IThread::ResumeOneThread(struct NaClAppThread *,
                              const struct NaClSignalContext *) {
  // TODO(eaeltsin): implement this.
}

void IThread::SetExceptionCatch(IThread::CatchFunc_t func, void *cookie) {
  NaClSignalHandlerAdd(SignalHandler);
  s_CatchFunc = func;
  s_CatchCookie = cookie;
}


}  // End of port namespace

