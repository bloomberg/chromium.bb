/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <windows.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/port/thread.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

/*
 * Define the OS specific portions of IThread interface.
 */

namespace {

const int kDBG_PRINTEXCEPTION_C = 0x40010006;

}  // namespace

namespace port {

static IThread::CatchFunc_t s_CatchFunc = NULL;
static void* s_CatchCookie = NULL;
static PVOID s_OldCatch = NULL;

enum PosixSignals {
  SIGINT  = 2,
  SIGQUIT = 3,
  SIGILL  = 4,
  SIGTRACE= 5,
  SIGBUS  = 7,
  SIGFPE  = 8,
  SIGKILL = 9,
  SIGSEGV = 11,
  SIGSTKFLT = 16,
};

static int8_t ExceptionToSignal(int ex) {
  switch (ex) {
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
      return SIGSEGV;

    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
      return SIGTRACE;

    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
      return SIGFPE;

    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
      return SIGILL;

    case EXCEPTION_STACK_OVERFLOW:
      return SIGSTKFLT;

    case CONTROL_C_EXIT:
      return SIGQUIT;

    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_INVALID_HANDLE:
      return SIGILL;
  }
  return SIGILL;
}

static LONG NTAPI ExceptionCatch(PEXCEPTION_POINTERS ep) {
  uint32_t id = static_cast<uint32_t>(GetCurrentThreadId());
  IThread* thread = IThread::Acquire(id);

  // This 2 lines is a fix for the bug:
  // 366: Linux GDB doesn't work for Chrome
  // http://code.google.com/p/nativeclient/issues/detail?id=366
  // When debug stub thread opens socket to listen (for RSP debugger),
  // it triggers some component to send DBG_PRINTEXCEPTION(with string
  // "swi_lsp: non-browser app; disable"), then VEH handler goes into wait
  // for debugger to resolve exception.
  // But debugger is not connected, and debug thread is not listening on
  // connection! It get stuck.
  // Ignoring this exception - for now - helps debug stub start on chrome.
  // Now it can listen on RSP connection and can get debugger connected etc.
  if (kDBG_PRINTEXCEPTION_C == ep->ExceptionRecord->ExceptionCode) {
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  // If we are not tracking this thread, then ignore it
  if (NULL == thread) return EXCEPTION_CONTINUE_SEARCH;

  int8_t sig = ExceptionToSignal(ep->ExceptionRecord->ExceptionCode);

  // Handle EXCEPTION_BREAKPOINT SEH/VEH handler special case:
  // Here instruction pointer from the CONTEXT structure points to the int3
  // instruction, not after the int3 instruction.
  // This is different from the hardware context, and (thus) different from
  // the context obtained via GetThreadContext on Windows and from signal
  // handler context on Linux.
  // See http://code.google.com/p/nativeclient/issues/detail?id=1730.
  // We adjust instruction pointer to match the hardware.
  if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
#if NACL_BUILD_SUBARCH == 64
    ep->ContextRecord->Rip += 1;
#else
    ep->ContextRecord->Eip += 1;
#endif
  }

  NaClSignalContextFromHandler(thread->GetContext(), ep->ContextRecord);
  if (NULL != s_CatchFunc) s_CatchFunc(id, sig, s_CatchCookie);
  NaClSignalContextToHandler(ep->ContextRecord, thread->GetContext());

  IThread::Release(thread);
  return EXCEPTION_CONTINUE_EXECUTION;
}

void IThread::SuspendOneThread(struct NaClAppThread *natp,
                               struct NaClSignalContext *context) {
  if (SuspendThread(natp->thread.tid) == -1) {
    NaClLog(LOG_FATAL, "IThread::SuspendOneThread: SuspendThread failed\n");
  }

  CONTEXT win_context;
  win_context.ContextFlags = CONTEXT_ALL;
  if (!GetThreadContext(natp->thread.tid, &win_context)) {
    NaClLog(LOG_FATAL, "IThread::SuspendOneThread: GetThreadContext failed\n");
  }
  NaClSignalContextFromHandler(context, &win_context);
}

void IThread::ResumeOneThread(struct NaClAppThread *natp,
                              const struct NaClSignalContext *context) {
  CONTEXT win_context;
  win_context.ContextFlags = CONTEXT_ALL;
  NaClSignalContextToHandler(&win_context, context);
  if (!SetThreadContext(natp->thread.tid, &win_context)) {
    NaClLog(LOG_FATAL, "IThread::ResumeOneThread: SetThreadContext failed\n");
  }

  if (ResumeThread(natp->thread.tid) == -1) {
    NaClLog(LOG_FATAL, "IThread::ResumeOneThread: ResumeThread failed\n");
  }
}

void IThread::SetExceptionCatch(IThread::CatchFunc_t func, void *cookie) {
  // Remove our old catch if there is one, this allows us to add again
  if (NULL != s_OldCatch) RemoveVectoredExceptionHandler(s_OldCatch);

  // Add the new one, at the front of the list
  s_OldCatch = AddVectoredExceptionHandler(1, ExceptionCatch);
  s_CatchFunc = func;
  s_CatchCookie = cookie;
}

}  // namespace port
