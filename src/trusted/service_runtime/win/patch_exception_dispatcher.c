/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <windows.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"


/*
 * Unlike Unix, Windows does much of its dispatching to fault handling
 * routines in user space, rather than in the kernel.  Hence
 * AddVectoredExceptionHandler() adds a function to a list maintained
 * in user space.  When a fault occurs, the kernel passes control to
 * an internal userland routine in NTDLL.DLL called
 * KiUserExceptionDispatcher, which in turn runs handlers registered
 * by AddVectoredExceptionHandler().  Furthermore,
 * KiUserExceptionDispatcher gets run even if no handlers have been
 * registered.  This is unlike Unix's signal() function, which
 * modifies a table of functions maintained in the kernel.  At the
 * same time, Windows has no equivalent of Unix's sigaltstack(), so
 * KiUserExceptionDispatcher runs on the stack that %esp/%rsp pointed
 * to at the time the fault occurred.
 *
 * This is really bad for x86-64 NaCl, because %rsp points into
 * untrusted address space.  KiUserExceptionDispatcher calls functions
 * internally, and it would be possible for untrusted code to hijack
 * these function returns by writing to the stack from another thread.
 *
 * To avoid this problem, we patch KiUserExceptionDispatcher to safely
 * terminate the process.  Of course, we are relying on an
 * implementation detail of Windows here.
 *
 * We make a number of assumptions:
 *
 *  * We assume that no-one is relying on catching faults inside
 *    trusted code.  This includes:
 *      * MSVC's __try/__except feature.
 *      * IsBadReadPtr() et al.  (Code that uses these functions is not
 *        fail-safe, so if IsBadReadPtr() is called on an invalid
 *        pointer, it is probably good that our patch makes it fail
 *        fast.)
 *    However, this is fixable.  In the future we may want to detect
 *    and handle trusted-code crashes.
 *
 *  * We assume that there are no jumps to instructions that we might
 *    be overwriting, except for the first instruction.  There is not
 *    really any way to verify this.  We minimise this risk by keeping
 *    our patch sequence short.  It is unlikely that there are any
 *    jumps to KiUserExceptionDispatcher+i for 0 < i < 4.
 *
 *  * We assume that no-one else is patching this routine in the same
 *    process.
 *
 *  * We assume that no-one catches and skips the HLT instruction.
 *
 *  * We assume that KiUserExceptionDispatcher really is the first
 *    address that the kernel passes control to in user space after a
 *    fault.
 *
 *  * The kernel still writes fault information to the untrusted stack.
 *    We assume that nothing sensitive is included in this stack
 *    frame, because untrusted code will probably be able to read this
 *    information.
 *
 * An alternative approach might be to use Windows' debugging
 * interfaces (DebugActiveProcess() etc.) to catch the fault before
 * any fault handler inside the sel_ldr process has run.  This would
 * have the advantage that the kernel would not write a stack frame to
 * the untrusted stack.  However, it would involve having an extra
 * helper process running.
 */

void NaClPatchWindowsExceptionDispatcher() {
  HMODULE ntdll;
  uint8_t *handler;
  DWORD old_prot;
  uint8_t *patch_bytes = NaCl_exception_dispatcher_exit_fast;
  size_t patch_size = (NaCl_exception_dispatcher_exit_fast_end -
                       NaCl_exception_dispatcher_exit_fast);

  ntdll = GetModuleHandleA("ntdll.dll");
  if (ntdll == NULL) {
    NaClLog(LOG_FATAL,
            "NaClPatchWindowsExceptionDispatcher: "
            "Failed to open ntdll.dll\n");
  }
  handler = (uint8_t *) GetProcAddress(ntdll, "KiUserExceptionDispatcher");
  if (handler == NULL) {
    NaClLog(LOG_FATAL,
            "NaClPatchWindowsExceptionDispatcher: "
            "GetProcAddress() failed to get KiUserExceptionDispatcher\n");
  }
  if (!VirtualProtect(handler, patch_size,
                      PAGE_EXECUTE_READWRITE, &old_prot)) {
    NaClLog(LOG_FATAL,
            "NaClPatchWindowsExceptionDispatcher: "
            "VirtualProtect() failed to make the routine writable\n");
  }
  memcpy(handler, patch_bytes, patch_size);
  if (!VirtualProtect(handler, patch_size, old_prot, &old_prot)) {
    NaClLog(LOG_FATAL,
            "NaClPatchWindowsExceptionDispatcher: "
            "VirtualProtect() failed to restore page permissions\n");
  }
}
