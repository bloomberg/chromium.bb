/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <windows.h>
#include <dbghelp.h>

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/tests/windows_crash_in_syscall/test_syscalls.h"

/*
 * This test case checks that an exception handler registered via
 * Windows' SetUnhandledExceptionFilter() API gets called if a crash
 * occurs inside a NaCl syscall handler.  On x86-64, we have to ensure
 * that the stack is set up in a way that Windows likes, otherwise
 * this exception handler does not get called.  For background, see
 * http://code.google.com/p/nativeclient/issues/detail?id=2237.
 */


static int32_t NotImplementedDecoder(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  printf("Error: entered an unexpected syscall!\n");
  fflush(stdout);
  _exit(1);
}

static int32_t CrashyTestSyscall(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  printf("Inside custom test 'invoke' syscall\n");
  fflush(stdout);

  /* Cause a crash.  This should cause ExceptionHandler() to be called. */
  *(volatile int *) 0 = 0;

  /* Should not reach here. */
  _exit(1);
}

#define MAX_SYMBOL_NAME_LENGTH 100

static void PrintSymbolForAddress(DWORD64 addr) {
  /*
   * Code adapted from Chromium's stack_trace_win.cc, in turn adapted
   * from an MSDN example:
   * http://msdn.microsoft.com/en-us/library/ms680578(VS.85).aspx
   */
  ULONG64 buffer[(sizeof(SYMBOL_INFO) +
                  MAX_SYMBOL_NAME_LENGTH * sizeof(wchar_t) +
                  sizeof(ULONG64) - 1) /
                 sizeof(ULONG64)];
  DWORD64 sym_displacement = 0;
  PSYMBOL_INFO symbol = (PSYMBOL_INFO) buffer;
  BOOL has_symbol;
  memset(buffer, 0, sizeof(buffer));

  SymInitialize(GetCurrentProcess(), NULL, TRUE);

  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYMBOL_NAME_LENGTH - 1;
  has_symbol = SymFromAddr(GetCurrentProcess(),
                           addr, &sym_displacement, symbol);
  if (has_symbol) {
    fprintf(stderr, "%s + 0x%x\n", symbol->Name, sym_displacement);
  } else {
    fprintf(stderr, "<no symbol>\n");
  }
}

/*
 * On x86-64 Windows, we expect the backtrace to contain the
 * following, with no gaps:
 *
 *   NaClSyscallCSegHook
 *   NaClSwitchSavingStackPtr
 *   NaClStartThreadInApp
 *
 * We could check for those names, but symbols are not always
 * available.  Instead we check for bogus stack frames below, which
 * the stack unwinder lets through.
 */
static void Backtrace(CONTEXT *initial_context) {
  int machine_type;
  CONTEXT context_for_frame = *initial_context;
  STACKFRAME64 frame = { 0 };
  int frame_number = 0;
  int failed = 0;

  fprintf(stderr, "Stack backtrace:\n");
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
  machine_type = IMAGE_FILE_MACHINE_AMD64;
  frame.AddrPC.Offset = initial_context->Rip;
  frame.AddrFrame.Offset = initial_context->Rbp;
  frame.AddrStack.Offset = initial_context->Rsp;
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  machine_type = IMAGE_FILE_MACHINE_I386;
  frame.AddrPC.Offset = initial_context->Eip;
  frame.AddrFrame.Offset = initial_context->Ebp;
  frame.AddrStack.Offset = initial_context->Esp;
#else
# error Unknown architecture
#endif
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Mode = AddrModeFlat;
  frame.Virtual = 0;

  while (1) {
    STACKFRAME64 previous_frame = frame;
    if (!StackWalk64(machine_type, GetCurrentProcess(), GetCurrentThread(),
                     &frame, &context_for_frame,
                     NULL, /* use ReadMemory() */
                     SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
      break;
    }
    fprintf(stderr, "#%i: ip=%p stack=%llx frame=%llx ",
            frame_number,
            frame.AddrPC.Offset,
            frame.AddrStack.Offset,
            frame.AddrFrame.Offset);
    PrintSymbolForAddress(frame.AddrPC.Offset);
    /*
     * Perform some sanity checks.  Windows' x86-64 stack unwinder
     * applies a fallback rule when it sees return addresses without
     * unwind info, but the fallback rule is for leaf functions.  This
     * causes the stack unwinder to report stack frames that cannot
     * possibly be valid in the Windows x86-64 ABI.  We check for such
     * frames here.  An error here would suggest that the unwind info
     * for NaClSwitchSavingStackPtr is wrong.
     *
     * The frame for a non-leaf function Foo() looks like this:
     *     32 bytes   shadow space (scratch space for Foo())
     *   ---- Foo()'s caller's AddrStack points here
     *      8 bytes   return address (points into Foo()'s caller)
     *      8 bytes   scratch space for Foo()
     *   ---- Foo()'s AddrFrame points here (unless the hardware exception
     *        occurred inside Foo(), in which case this is less well-defined)
     *   16*n bytes   scratch space for Foo() (for some n >= 0)
     *     32 bytes   shadow space (scratch space for Foo()'s callees)
     *   ---- Foo()'s rsp and AddrStack point here
     *
     * The frame for a leaf function Bar() that never adjusts rsp
     * looks like this:
     *     32 bytes   shadow space (scratch space for Bar())
     *      8 bytes   return address (points into Bar()'s caller)
     *   ---- Bar()'s rsp and AddrStack points here
     *      8 bytes   not usable by Bar() at all!
     *   ---- Bar()'s AddrFrame points here
     */
    if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64) {
      /* frame_size must be signed for the check to be useful. */
      long long frame_size = frame.AddrFrame.Offset - frame.AddrStack.Offset;
      if (frame_number > 0 && frame_size < 32) {
        fprintf(stderr, "Error: frame_size=%i, which is too small\n",
                frame_size);
        failed = 1;
      }
      if (frame_number > 1 &&
          frame.AddrStack.Offset != previous_frame.AddrFrame.Offset + 16) {
        fprintf(stderr, "Error: stack does not fit with previous frame\n");
        failed = 1;
      }
    }
    frame_number++;
  }
  if (failed) {
    _exit(1);
  }
}

static LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *exc_info) {
  printf("Inside exception handler, as expected\n");
  Backtrace(exc_info->ContextRecord);
  printf("Stack backtrace passed sanity checks\n");
  /*
   * Continuing is what Breakpad does, but this should cause the
   * process to exit with an exit status that is appropriate for the
   * type of exception.  We want to test that ExceptionHandler() does
   * not get called twice, since that does not work with Chrome's
   * embedding of Breakpad.
   */
  return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv) {
  struct NaClApp app;
  struct GioMemoryFileSnapshot gio_file;
  /* sel_ldr_standard.c currently requires at least 1 argument. */
  char *untrusted_argv[] = {"blah"};
  struct NaClSyscallTableEntry syscall_table[NACL_MAX_SYSCALLS] = {{0}};
  int index;
  for (index = 0; index < NACL_MAX_SYSCALLS; index++) {
    syscall_table[index].handler = NotImplementedDecoder;
  }
  syscall_table[CRASHY_TEST_SYSCALL].handler = CrashyTestSyscall;

  /* Turn off buffering to aid debugging. */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClAllModulesInit();

  NaClFileNameForValgrind(argv[1]);
  if (GioMemoryFileSnapshotCtor(&gio_file, argv[1]) == 0) {
    NaClLog(LOG_FATAL, "GioMemoryFileSnapshotCtor() failed\n");
  }

  if (!NaClAppWithSyscallTableCtor(&app, syscall_table)) {
    NaClLog(LOG_FATAL, "NaClAppCtor() failed\n");
  }

  if (NaClAppLoadFile((struct Gio *) &gio_file, &app) != LOAD_OK) {
    NaClLog(LOG_FATAL, "NaClAppLoadFile() failed\n");
  }

  if (NaClAppPrepareToLaunch(&app) != LOAD_OK) {
    NaClLog(LOG_FATAL, "NaClAppPrepareToLaunch() failed\n");
  }

  SetUnhandledExceptionFilter(ExceptionHandler);

  if (!NaClCreateMainThread(&app, 1, untrusted_argv, NULL)) {
    NaClLog(LOG_FATAL, "NaClCreateMainThread() failed\n");
  }

  NaClWaitForMainThreadToExit(&app);

  NaClLog(LOG_FATAL, "The exit syscall is not supposed to be callable\n");

  return 1;
}
