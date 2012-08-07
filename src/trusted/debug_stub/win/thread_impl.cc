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
    case EXCEPTION_PRIV_INSTRUCTION:
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
  struct NaClSignalContext context;

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

  NaClSignalContextFromHandler(&context, ep->ContextRecord);
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

    uint32_t id = static_cast<uint32_t>(GetCurrentThreadId());
    IThread* thread = IThread::Acquire(id);
    *thread->GetContext() = context;
    // Handle EXCEPTION_BREAKPOINT SEH/VEH handler special case:
    // Here instruction pointer from the CONTEXT structure points to the int3
    // instruction, not after the int3 instruction.
    // This is different from the hardware context, and (thus) different from
    // the context obtained via GetThreadContext on Windows and from signal
    // handler context on Linux.
    // See http://code.google.com/p/nativeclient/issues/detail?id=1730.
    // We adjust instruction pointer to match the hardware.
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
      thread->GetContext()->prog_ctr += 1;
    }

    int8_t sig = ExceptionToSignal(ep->ExceptionRecord->ExceptionCode);
    if (NULL != s_CatchFunc) s_CatchFunc(id, sig, s_CatchCookie);
    NaClSignalContextToHandler(ep->ContextRecord, thread->GetContext());

    IThread::Release(thread);
    // TODO(eaeltsin): remove this hack!
    while (CompareAndSwap(&g_is_processing_signal, 1, 0) != 1) {
      // Spin.
    }
    return EXCEPTION_CONTINUE_EXECUTION;
  } else {
    // Do not attempt to debug crashes in trusted code.
    return EXCEPTION_CONTINUE_SEARCH;
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
