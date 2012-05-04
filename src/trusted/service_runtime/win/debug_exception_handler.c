/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"

#include <stddef.h>
#include <stdio.h>
#include <windows.h>

#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_exception.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/win/thread_handle_map.h"


/*
 * This struct is passed in a message from the main sel_ldr process to
 * the debug exception handler process to communicate the address of
 * service_runtime's global arrays, nacl_thread and nacl_thread_ids.
 */
struct StartupInfo {
  struct NaClAppThread **nacl_thread;
  uint32_t *nacl_thread_ids;
};


static int HandleException(const struct StartupInfo *startup_info,
                           HANDLE process_handle, DWORD windows_thread_id,
                           HANDLE thread_handle);


static BOOL GetAddrProtection(HANDLE process_handle, uintptr_t addr,
                              int *protect) {
  MEMORY_BASIC_INFORMATION memory_info;
  if (VirtualQueryEx(process_handle, (void *) addr,
                     &memory_info, sizeof(memory_info))
      != sizeof(memory_info)) {
    return FALSE;
  }
  *protect = memory_info.Protect;
  return TRUE;
}

static BOOL ReadProcessMemoryChecked(HANDLE process_handle,
                                     const void *remote_addr,
                                     void *local_addr, size_t size) {
  size_t bytes_copied;
  return (ReadProcessMemory(process_handle, remote_addr, local_addr,
                            size, &bytes_copied)
          && bytes_copied == size);
}

static BOOL WriteProcessMemoryChecked(HANDLE process_handle, void *remote_addr,
                                      const void *local_addr, size_t size) {
  size_t bytes_copied;
  /*
   * WriteProcessMemory() does not always respect page permissions, so
   * we first manually check permissions to ensure that we are not
   * overwriting untrusted code.  For background, see
   * http://code.google.com/p/nativeclient/issues/detail?id=2572.  The
   * debuggee process should be suspended and unable to change page
   * mappings at the moment, so there should be no TOCTTOU race
   * condition with this check.
   */
  uintptr_t page_start = NaClTruncPage((uintptr_t) remote_addr);
  uintptr_t page_end = NaClRoundPage((uintptr_t) remote_addr + size);
  uintptr_t addr;
  for (addr = page_start; addr < page_end; addr += NACL_PAGESIZE) {
    int protect;
    if (!GetAddrProtection(process_handle, addr, &protect) ||
        protect != PAGE_READWRITE) {
      return FALSE;
    }
  }
  return (WriteProcessMemory(process_handle, remote_addr, local_addr,
                             size, &bytes_copied)
          && bytes_copied == size);
}


#define READ_MEM(process_handle, remote_ptr, local_ptr) \
  (sizeof(*remote_ptr) == sizeof(*local_ptr) && \
   ReadProcessMemoryChecked(process_handle, remote_ptr, local_ptr, \
                            sizeof(*local_ptr)))

#define WRITE_MEM(process_handle, remote_ptr, local_ptr) \
  (sizeof(*remote_ptr) == sizeof(*local_ptr) && \
   WriteProcessMemoryChecked(process_handle, remote_ptr, local_ptr, \
                             sizeof(*local_ptr)))


/*
 * NaClDebugExceptionHandlerRun() below handles debug events in NaCl program.
 * CREATE_PROCESS, CREATE_THREAD and EXIT_THREAD events are used
 * to store thread handles. These thread handles are needed to get and set
 * thread context.
 *
 * EXCEPTION debug events are classified into 3 types. The BREAKPOINT event
 * that happens at attaching debugger to the program. All other trusted
 * exceptions are not handled. All untrusted exceptions are handled if
 * possible. The exception is called untrusted in both cs and ds are
 * non-standard. Debugger reads exception handler address and stack from NaCl
 * process and transfers control to the handler by changing thread context and
 * writing exception data to the NaCl process. Values of all registers are not
 * changed with the exception of eip and esp. When all data is written and
 * context have changed, debugger finishes exception debug event handling and
 * all threads in NaCl application continue execution. If anything goes wrong,
 * debugger doesn't handle exception and program terminates.
 *
 * Debugger have a special logic to prevent nested exceptions. It sets special
 * flag in NaCl program that must be cleared by exception handler. If exception
 * happens before that, debugger doesn't handle the exception and program
 * terminates.
 */

void NaClDebugExceptionHandlerRun(HANDLE process_handle,
                                  const void *info, size_t info_size) {
  const struct StartupInfo *startup_info = info;
  DEBUG_EVENT debug_event;
  int process_exited = 0;
  int error = 0;
  DWORD continue_status;
  ThreadHandleMap *map;
  HANDLE thread_handle;
  DWORD exception_code;

  if (info_size != sizeof(struct StartupInfo)) {
    return;
  }

  map = CreateThreadHandleMap();
  while (!process_exited) {
    if (!WaitForDebugEvent(&debug_event, INFINITE)) {
      error = 1;
      break;
    }
    continue_status = DBG_CONTINUE;
    switch(debug_event.dwDebugEventCode) {
      case OUTPUT_DEBUG_STRING_EVENT:
      case UNLOAD_DLL_DEBUG_EVENT:
        break;
      case CREATE_PROCESS_DEBUG_EVENT:
        CloseHandle(debug_event.u.CreateProcessInfo.hFile);
        if (!ThreadHandleMapPut(map, debug_event.dwThreadId,
                                debug_event.u.CreateProcessInfo.hThread)) {
          error = 1;
        }
        break;
      case LOAD_DLL_DEBUG_EVENT:
        CloseHandle(debug_event.u.LoadDll.hFile);
        break;
      case CREATE_THREAD_DEBUG_EVENT:
        if (!ThreadHandleMapPut(map, debug_event.dwThreadId,
                                debug_event.u.CreateThread.hThread)) {
          error = 1;
        }
        break;
      case EXIT_THREAD_DEBUG_EVENT:
        ThreadHandleMapDelete(map, debug_event.dwThreadId);
        break;
      case EXIT_PROCESS_DEBUG_EVENT:
        process_exited = 1;
        break;
      case EXCEPTION_DEBUG_EVENT:
        continue_status = DBG_EXCEPTION_NOT_HANDLED;
        thread_handle = ThreadHandleMapGet(map, debug_event.dwThreadId);
        if (thread_handle == INVALID_HANDLE_VALUE) {
          error = 1;
        } else {
          exception_code =
              debug_event.u.Exception.ExceptionRecord.ExceptionCode;
          if (HandleException(startup_info, process_handle,
                              debug_event.dwThreadId, thread_handle)) {
            continue_status = DBG_CONTINUE;
          } else if (exception_code == EXCEPTION_BREAKPOINT) {
            /*
             * When we attach to a process, the Windows debug API
             * triggers a breakpoint in the process by injecting a
             * thread to run NTDLL's DbgUiRemoteBreakin, which calls
             * DbgBreakPoint.  We need to resume the process from this
             * breakpoint.
             *
             * Resuming the breakpoint with DBG_EXCEPTION_NOT_HANDLED
             * sometimes works, but not on all versions of Windows.
             * This creates a userland exception which is not handled
             * in older versions of Windows, and it does not get
             * handled successfully with NaCl's
             * KiUserExceptionDispatcher patch because TLS is not set
             * up this injected thread (see
             * http://code.google.com/p/nativeclient/issues/detail?id=2726).
             */
            continue_status = DBG_CONTINUE;
          }
        }
        break;
    }
    if (error) {
      break;
    }
    if (!ContinueDebugEvent(
             debug_event.dwProcessId,
             debug_event.dwThreadId,
             continue_status)) {
      break;
    }
  }
  DestroyThreadHandleMap(map);
  if (error) {
    TerminateProcess(process_handle, -1);
  }
}


/*
 * When GetThreadIndex() returns true, the given thread was associated
 * with a NaClAppThread, and *nacl_thread_index has been filled out
 * with the index of the NaClAppThread.
 *
 * On x86-32, GetThreadIndex() only returns true if the thread faulted
 * while running untrusted code.
 *
 * On x86-64, GetThreadIndex() can return true if the thread faulted
 * while running trusted code, so an additional check is needed to
 * determine whether the thread faulted in trusted or untrusted code.
 */
#if NACL_BUILD_SUBARCH == 32
static BOOL GetThreadIndex(const struct StartupInfo *startup_info,
                           HANDLE process_handle, HANDLE thread_handle,
                           const CONTEXT *regs,
                           uint32_t windows_thread_id,
                           uint32_t *nacl_thread_index) {
  LDT_ENTRY cs_entry;
  LDT_ENTRY ds_entry;
  DWORD cs_limit;
  DWORD ds_limit;

  GetThreadSelectorEntry(thread_handle, regs->SegCs, &cs_entry);
  cs_limit = cs_entry.LimitLow + (((int)cs_entry.HighWord.Bits.LimitHi) << 16);
  GetThreadSelectorEntry(thread_handle, regs->SegDs, &ds_entry);
  ds_limit = ds_entry.LimitLow + (((int)ds_entry.HighWord.Bits.LimitHi) << 16);
  /* Segment limits are 4Gb in trusted code */
  if (cs_limit == 0xfffff || ds_limit == 0xfffff) {
    return FALSE;
  }
  *nacl_thread_index = regs->SegGs >> 3;
  return TRUE;
}
#else
/*
 * On x86-64, if we were running in the faulting thread in the sel_ldr
 * process, we could read a TLS variable to find the current NaCl
 * thread's index, as nacl_syscall_64.S does.  However, it is
 * difficult for an out-of-process debugger to read TLS variables on
 * x86-64.  We could record the lpThreadLocalBase field of the events
 * CREATE_THREAD_DEBUG_INFO and CREATE_PROCESS_DEBUG_INFO to get the
 * TEB base address when a new thread is created, but this gets
 * complicated.
 *
 * The implementation below is simpler but slower, because it copies
 * all of the nacl_thread_ids global array.
 */
static BOOL GetThreadIndex(const struct StartupInfo *startup_info,
                           HANDLE process_handle, HANDLE thread_handle,
                           const CONTEXT *regs,
                           uint32_t windows_thread_id,
                           uint32_t *nacl_thread_index) {
  BOOL success = FALSE;
  size_t array_size = sizeof(nacl_thread_ids);
  uint32_t *copy;
  uint32_t index;

  copy = malloc(array_size);
  if (copy == NULL) {
    return FALSE;
  }
  if (!ReadProcessMemoryChecked(process_handle, startup_info->nacl_thread_ids,
                                copy, sizeof(nacl_thread_ids))) {
    goto done;
  }
  for (index = 0; index < NACL_ARRAY_SIZE(nacl_thread_ids); index++) {
    if (copy[index] == windows_thread_id) {
      *nacl_thread_index = index;
      success = TRUE;
      goto done;
    }
  }
 done:
  free(copy);
  return success;
}
#endif

static BOOL HandleException(const struct StartupInfo *startup_info,
                            HANDLE process_handle, DWORD windows_thread_id,
                            HANDLE thread_handle) {
  CONTEXT context;
  uint32_t nacl_thread_index;
  uintptr_t addr_space_size;
  uint32_t exception_stack;
  uint32_t exception_frame_end;
  uint32_t exception_flag;
  struct NaClExceptionFrame new_stack;
  /*
   * natp_remote points into the debuggee process's address space, so
   * cannot be dereferenced directly.  We use a pointer type for the
   * convenience of calculating field offsets and sizes.
   */
  struct NaClAppThread *natp_remote;
  struct NaClAppThread appthread_copy;
  struct NaClApp app_copy;
  uint32_t context_user_addr;

  context.ContextFlags = CONTEXT_SEGMENTS | CONTEXT_INTEGER | CONTEXT_CONTROL;
  if (!GetThreadContext(thread_handle, &context)) {
    return FALSE;
  }

  if (!GetThreadIndex(startup_info, process_handle, thread_handle, &context,
                      windows_thread_id, &nacl_thread_index)) {
    /*
     * We can get here if the faulting thread is running trusted code
     * (an expected situation) or if an unexpected error occurred.
     */
    return FALSE;
  }

  if (!READ_MEM(process_handle, startup_info->nacl_thread + nacl_thread_index,
                &natp_remote)) {
    return FALSE;
  }
  if (natp_remote == NULL) {
    /*
     * This means the nacl_thread and nacl_thread_ids arrays do not
     * match up.  TODO(mseaborn): Complain more noisily about such
     * unexpected cases and terminate the NaCl process.
     */
    return FALSE;
  }
  /*
   * We make copies of the debuggee process's NaClApp and
   * NaClAppThread structs.  We avoid passing these copies to
   * functions such as NaClIsUserAddr() in case these functions evolve
   * to expect a real NaClApp that contains pointers to in-process
   * data structures.
   */
  if (!READ_MEM(process_handle, natp_remote, &appthread_copy)) {
    return FALSE;
  }
  if (!READ_MEM(process_handle, appthread_copy.nap, &app_copy)) {
    return FALSE;
  }

  addr_space_size = (uintptr_t) 1U << app_copy.addr_bits;
#if NACL_BUILD_SUBARCH == 64
  /*
   * Check whether the crash occurred in trusted or untrusted code.
   * TODO(mseaborn): If the instruction pointer is inside the guard
   * regions, we might want to treat that as an untrusted crash, since
   * relative jumps can jump to the guard regions.
   */
  if (context.Rip < app_copy.mem_start ||
      context.Rip >= app_copy.mem_start + addr_space_size) {
    /* The fault is inside trusted code, so do not handle it. */
    return FALSE;
  }
#endif

  exception_stack = appthread_copy.exception_stack;
  exception_flag = appthread_copy.exception_flag;
  if (exception_flag) {
    return FALSE;
  }
  exception_flag = 1;
  if (!WRITE_MEM(process_handle, &natp_remote->exception_flag,
                 &exception_flag)) {
    return FALSE;
  }

  /*
   * Sanity check: Ensure the entry point is properly aligned.  We do
   * not expect this to fail because alignment was checked by the
   * syscall when the function was registered.
   */
  if (app_copy.exception_handler % NACL_INSTR_BLOCK_SIZE != 0) {
    return FALSE;
  }

  if (exception_stack == 0) {
#if NACL_BUILD_SUBARCH == 32
    exception_stack = context.Esp;
#else
    exception_stack = (uint32_t) context.Rsp;
#endif
    exception_stack -= NACL_STACK_RED_ZONE;
  }
  /*
   * Calculate the position of the exception stack frame, with
   * suitable alignment.
   */
  exception_stack -=
      sizeof(struct NaClExceptionFrame) - NACL_STACK_PAD_BELOW_ALIGN;
  exception_stack = exception_stack & ~NACL_STACK_ALIGN_MASK;
  exception_stack -= NACL_STACK_PAD_BELOW_ALIGN;
  /*
   * Ensure that the exception frame would be written within the
   * bounds of untrusted address space.
   */
  exception_frame_end = exception_stack + sizeof(struct NaClExceptionFrame);
  if (exception_frame_end < exception_stack) {
    /* Unsigned overflow. */
    return FALSE;
  }
  if (exception_frame_end > addr_space_size) {
    return FALSE;
  }

  new_stack.return_addr = 0;
  context_user_addr = exception_stack +
      offsetof(struct NaClExceptionFrame, context);
#if NACL_BUILD_SUBARCH == 32
  new_stack.context_ptr = context_user_addr;
  new_stack.context.prog_ctr = context.Eip;
  new_stack.context.stack_ptr = context.Esp;
  new_stack.context.frame_ptr = context.Ebp;
#else
  /*
   * Explicit downcast of %rip, %rsp, and %rbp required as we're truncating
   * values for portability.
   */
  new_stack.context.prog_ctr = (uint32_t) context.Rip;
  new_stack.context.stack_ptr = (uint32_t) context.Rsp;
  new_stack.context.frame_ptr = (uint32_t) context.Rbp;
#endif
  if (!WRITE_MEM(process_handle,
                 (struct NaClExceptionFrame *) (app_copy.mem_start
                                                + exception_stack),
                 &new_stack)) {
    return FALSE;
  }

#if NACL_BUILD_SUBARCH == 32
  context.Eip = app_copy.exception_handler;
  context.Esp = exception_stack;
#else
  context.Rdi = context_user_addr;  /* Argument 1 */
  context.Rip = app_copy.mem_start + app_copy.exception_handler;
  context.Rsp = app_copy.mem_start + exception_stack;
  /*
   * The x86-64 sandbox allows %rsp/%rbp to point temporarily to the
   * lower 4GB of address space.  It should not be possible to fault
   * while %rsp/%rbp is in this state; otherwise, we have safety
   * problems when the debug stub is not enabled.  For portability,
   * and to be on the safe side, we reset %rbp to zero in untrusted
   * address space.
   */
  context.Rbp = app_copy.mem_start;
#endif

  context.EFlags &= ~NACL_X86_DIRECTION_FLAG;
  return SetThreadContext(thread_handle, &context);
}

int NaClDebugExceptionHandlerEnsureAttached(struct NaClApp *nap) {
  if (nap->attach_debug_exception_handler_func == NULL) {
    /*
     * No callback was provided, so we assume that the debug exception
     * handler was attached during process startup.
     */
    return 1;
  }
  if (nap->debug_exception_handler_state
      == NACL_DEBUG_EXCEPTION_HANDLER_NOT_STARTED) {
    struct StartupInfo info;
    info.nacl_thread = nacl_thread;
    info.nacl_thread_ids = nacl_thread_ids;
    if (nap->attach_debug_exception_handler_func(&info, sizeof(info))) {
      nap->debug_exception_handler_state = NACL_DEBUG_EXCEPTION_HANDLER_STARTED;
    } else {
      /* Attaching the exception handler failed; don't try again later. */
      nap->debug_exception_handler_state = NACL_DEBUG_EXCEPTION_HANDLER_FAILED;
    }
  }
  return (nap->debug_exception_handler_state
          == NACL_DEBUG_EXCEPTION_HANDLER_STARTED);
}

/*
 * TODO(mseaborn): Remove this function.  It is provided because
 * Chromium is currently using it, but Chromium will be switched over
 * to using NaClDebugExceptionHandlerRun().
 */
int NaClDebugLoop(HANDLE process_handle, DWORD *exit_code) {
  /*
   * This assumes that nacl_thread (a global array) is at the same
   * address in the debuggee process as in this debugger process.
   * This assumption is not always true, which is why this function is
   * scheduled to be removed!
   */
  struct StartupInfo info;
  info.nacl_thread = nacl_thread;
  info.nacl_thread_ids = nacl_thread_ids;

  NaClDebugExceptionHandlerRun(process_handle, &info, sizeof(info));
  /*
   * Chromium does not look at *exit_code and the return value.  We
   * provide dummy values here.
   */
  *exit_code = 0;
  return DEBUG_EXCEPTION_HANDLER_SUCCESS;
}
