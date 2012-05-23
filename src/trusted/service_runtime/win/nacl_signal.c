/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

static PVOID s_VEH = NULL;

/*
 * Microsoft's compilers generate code which uses the Structured
 * Exception Handling mechanism, which will call through the
 * generic exception system.  Microsoft uses magic numbers to
 * differentiate these cases from "real" exceptions.  For example
 * the 'C/C++' exception code magic is the bytes 0xE0 + 'msc', while
 * the managed code (such as C#) is the bytes 0xE0 + 'COM'.
 * NOTE:  This can only happen in trusted code, since the untrusted
 * code does not have access to the system call.
 * For an example of these concepts see:
    http://blogs.microsoft.co.il/blogs/sasha/archive/2008/08/20/
    converting-win32-and-c-exceptions-to-managed-exceptions.aspx
 */
#define CLR_EXCEPTION 0xE0434F4D    /* Manged code exception */
#define CPP_EXCEPTION 0xE06D7363    /* C/C++ exception  */

/*
 * On Windows, we do not have an ALT stack available, so we do not
 * allocate one.  This makes signal handling in Windows unsafe.
 */
int NaClSignalStackAllocate(void **result) {
  UNREFERENCED_PARAMETER(result);
  return 1; /* Success */
}

void NaClSignalStackFree(void *stack) {
  UNREFERENCED_PARAMETER(stack);
}

void NaClSignalStackRegister(void *stack) {
  UNREFERENCED_PARAMETER(stack);
}

void NaClSignalStackUnregister(void) {
}

static int ExceptionToSignal(int ex) {
  switch (ex) {
    /* Data access violations are typically SIGSEGV in Posix */
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:

    /* We treat a bad handle as if we access unreachable data */
    case EXCEPTION_INVALID_HANDLE:
      return SIGSEGV;

    /* Map break points and single step to SIGTRACE */
    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
      return SIGTRACE;

    /* Map all int and float exceptions to SIGFPE */
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
      return SIGFPE;

    /* Map a stack overflow to SIGSTKFLT */
    case EXCEPTION_STACK_OVERFLOW:
      return SIGSTKFLT;

    /* Map CTRL-C to SIGQUIT */
    case CONTROL_C_EXIT:
      return SIGQUIT;

    /* Map instruction errors to SIGILL */
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_INVALID_DISPOSITION:
      return SIGILL;
  }
  /* We default to SIGSEGV since that's the most common error. */
  return SIGSEGV;
}

static LONG NTAPI ExceptionCatch(PEXCEPTION_POINTERS ep) {
  int sig = ExceptionToSignal(ep->ExceptionRecord->ExceptionCode);

  /*
   * We ignore exceptions which may be handled by the language's normal
   * mechanism, allowing them to pass through to the OS which will
   * look for a matching "catch".  If one is not found, a new exception
   * will be raised with a different exception code, and we can process
   * it then.
   */
  switch(ep->ExceptionRecord->ExceptionCode) {
    case CLR_EXCEPTION:
    case CPP_EXCEPTION:
      return EXCEPTION_CONTINUE_SEARCH;
  }

  /* If we have handled the exception and are ready to return, do so */
  if (NaClSignalHandlerFind(sig, ep->ContextRecord) == NACL_SIGNAL_RETURN) {
      return EXCEPTION_CONTINUE_EXECUTION;
  }

  /* Otherwise we let the OS handle it. */
  return EXCEPTION_CONTINUE_SEARCH;
}

void NaClSignalHandlerInitPlatform(void) {
  /* Add this exception handler to the front of the list */
  s_VEH = AddVectoredExceptionHandler(1, ExceptionCatch);
}

void NaClSignalHandlerFiniPlatform(void) {
  /* Remove our old catch if there is one, this allows us to add again */
  if (NULL != s_VEH) RemoveVectoredExceptionHandler(s_VEH);
}

void NaClSignalAssertNoHandlers() {
  /*
   * Windows has no direct equivalent of Unix signal handlers.  There
   * is no interface for checking that no fault handlers are registered.
   */
}
