/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_WIN_STACK_UNWIND_STOP_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_WIN_STACK_UNWIND_STOP_H_ 1

#include "native_client/src/include/portability.h"

/*
 * The wrappers below are used as follows:
 *
 *   NACL_WINDOWS_EXCEPTION_BEGIN;
 *   ...
 *   NACL_WINDOWS_EXCEPTION_END;
 *
 * We do this for x86-64 Windows because this platform relies on being
 * able to unwind the stack to run exception handlers, including the
 * last-chance handler registered via SetUnhandledExceptionFilter(),
 * which Breakpad relies on.  However, Windows cannot unwind the call
 * to NaClSyscallCSegHook().
 *
 * As a workaround to make Breakpad work on x86-64 Windows for crashes
 * inside NaCl syscalls, we catch exceptions from there explicitly and
 * pass them to the SetUnhandledExceptionFilter() callback.
 *
 * A cleaner solution might be to provide unwind info for
 * NaClSyscallSeg, but that is tricky because the assembler we are
 * using to assemble that code (GNU binutils) does not handle x86-64
 * Windows unwind info.
 *
 * We do this only on x86-64 because it is not needed on x86-32 and it
 * would have a performance cost on x86-32, where the exception
 * handler chain is stored in %fs:0 and is not inferred by unwinding
 * the stack.
 */

#if (NACL_WINDOWS && NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && \
     NACL_BUILD_SUBARCH == 64)

# include <windows.h>

static INLINE LPTOP_LEVEL_EXCEPTION_FILTER GetUnhandledExceptionFilter(void) {
  /*
   * Windows does not provide a GetUnhandledExceptionFilter()
   * function, so simulate it by unsetting the handler (getting the
   * previous handler) and then re-setting the handler.
   *
   * TODO(mseaborn): This could cause crashes to fail to be handled if
   * multiple crashes occur concurrently in different threads.
   */
  LPTOP_LEVEL_EXCEPTION_FILTER handler = SetUnhandledExceptionFilter(NULL);
  SetUnhandledExceptionFilter(handler);
  return handler;
}

static INLINE LONG NaClExceptionFilter(EXCEPTION_POINTERS *exc_info) {
  LPTOP_LEVEL_EXCEPTION_FILTER handler = GetUnhandledExceptionFilter();
  if (handler != NULL) {
    handler(exc_info);
  }
  /*
   * This return code causes Windows to continue to unwind the stack.
   * Inside a NaCl syscall, the unwinding will fail and the process
   * will exit with the usual exit code for the fault type.
   */
  return EXCEPTION_CONTINUE_SEARCH;
}

# define NACL_WINDOWS_EXCEPTION_BEGIN \
  do { \
    __try {
# define NACL_WINDOWS_EXCEPTION_END \
    } __except (NaClExceptionFilter(GetExceptionInformation())) { \
      NaClLog(LOG_FATAL, "blah!\n"); \
    } \
  } while (0)

#else

# define NACL_WINDOWS_EXCEPTION_BEGIN do {
# define NACL_WINDOWS_EXCEPTION_END } while (0)

#endif

#endif
