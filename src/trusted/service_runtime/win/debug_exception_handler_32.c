/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"
#include "native_client/src/trusted/service_runtime/win/thread_handle_map.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/arch/x86_32/sel_rt_32.h"
#include <windows.h>
#include <stdio.h>

static int HandleBreakpointException(HANDLE thread_handle);
static int HandleException(HANDLE process_handle, HANDLE thread_handle);

struct ExceptionFrame {
  uint32_t return_addr;
  uint32_t prog_ctr;
  uint32_t stack_ptr;
};


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
  return (WriteProcessMemory(process_handle, remote_addr, local_addr,
                             size, &bytes_copied)
          && bytes_copied == size);
}

/*
 * DebugLoop below handles debug events in NaCl program.
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

int NaClDebugLoop(HANDLE process_handle, DWORD *exit_code) {
  DEBUG_EVENT debug_event;
  int process_exited = 0;
  int error = 0;
  DWORD continue_status;
  ThreadHandleMap *map;
  HANDLE thread_handle;
  DWORD exception_code;
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
          if (HandleException(process_handle, thread_handle)) {
            continue_status = DBG_CONTINUE;
          } else if (exception_code == EXCEPTION_BREAKPOINT &&
                     HandleBreakpointException(thread_handle)) {
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
    return DEBUG_EXCEPTION_HANDLER_ERROR;
  }
  GetExitCodeProcess(process_handle, exit_code);
  return DEBUG_EXCEPTION_HANDLER_SUCCESS;
}

/*
 * INT3 is not allowed in NaCl code but breakpoint exception
 * always happens when debugger attaches to the program or
 * starts program under debugging. This is the only exception
 * that must be handled in trusted code.
 */
static BOOL HandleBreakpointException(HANDLE thread_handle) {
  CONTEXT context;
  LDT_ENTRY cs_entry;
  DWORD cs_limit;
  context.ContextFlags = CONTEXT_CONTROL;
  if (!GetThreadContext(thread_handle, &context)) {
    return FALSE;
  }
  GetThreadSelectorEntry(thread_handle, context.SegCs, &cs_entry);
  cs_limit = cs_entry.LimitLow + (((int)cs_entry.HighWord.Bits.LimitHi) << 16);
  /* Segment limits are 4Gb in trusted code */
  return cs_limit == 0xfffff;
}

static BOOL GetExceptionHandlingInfo(HANDLE process_handle,
                                     int base,
                                     CONTEXT *context,
                                     void **nacl_context,
                                     DWORD *exception_handler,
                                     DWORD *exception_stack,
                                     DWORD *exception_flag) {
  void* nacl_user;
  void* nap_exception_handler;
  DWORD handler_data[2];
  int nacl_thread_id = context->SegGs / 8;

  if (!ReadProcessMemoryChecked(
           process_handle,
           (LPCVOID)(base + NACL_TRAMPOLINE_END -
                     NACL_SYSCALL_BLOCK_SIZE - sizeof(nacl_user)),
           &nacl_user, sizeof(nacl_user))) {
    return FALSE;
  }
  if (!ReadProcessMemoryChecked(
           process_handle,
           (LPCVOID)((DWORD_PTR)nacl_user +
                     sizeof(*nacl_context) * nacl_thread_id),
           nacl_context, sizeof(*nacl_context))) {
    return FALSE;
  }
  if (!ReadProcessMemoryChecked(
           process_handle,
           (LPCVOID)((DWORD_PTR)*nacl_context +
                     NACL_THREAD_CONTEXT_EXCEPTION_HANDLER_OFFSET),
           &handler_data, sizeof(handler_data))) {
    return FALSE;
  }
  *exception_stack = handler_data[0];
  *exception_flag = handler_data[1];
  if (*exception_stack == 0) {
    *exception_stack = context->Esp;
  }
  if (!ReadProcessMemoryChecked(
           process_handle,
           (LPCVOID)(base + NACL_TRAMPOLINE_END - 1 - sizeof(nacl_user) -
                     NACL_SYSCALL_BLOCK_SIZE - sizeof(nap_exception_handler)),
           &nap_exception_handler, sizeof(nap_exception_handler))) {
    return FALSE;
  }
  if (!ReadProcessMemoryChecked(
           process_handle,
           (LPCVOID)nap_exception_handler,
           exception_handler, sizeof(*exception_handler))) {
    return FALSE;
  }
  return TRUE;
}

static BOOL SetExceptionFlag(HANDLE process_handle, void* nacl_context) {
  DWORD exception_flag = 1;
  if (!WriteProcessMemoryChecked(
            process_handle,
            (LPVOID)((DWORD_PTR)nacl_context + 2 * sizeof(DWORD) +
                     NACL_THREAD_CONTEXT_EXCEPTION_HANDLER_OFFSET),
            &exception_flag, sizeof(exception_flag))) {
    return FALSE;
  }
  return TRUE;
}

static BOOL IsExceptionHandlerValid(HANDLE process_handle, DWORD base,
                                    DWORD cs_limit, DWORD ds_limit,
                                    DWORD exception_handler,
                                    DWORD exception_stack) {
  uint32_t code_segment_size = (cs_limit + 1) << 12;
  uint32_t data_segment_size = (ds_limit + 1) << 12;
  uint32_t exception_frame_end;

  /* check exception_handler value */
  if (exception_handler >= code_segment_size) {
    return FALSE;
  }
  if (exception_handler % NACL_INSTR_BLOCK_SIZE != 0) {
    return FALSE;
  }

  exception_frame_end = exception_stack + sizeof(struct ExceptionFrame);
  if (exception_frame_end < exception_stack) {
    /* Unsigned overflow. */
    return FALSE;
  }
  if (exception_frame_end > data_segment_size) {
    return FALSE;
  }
  return TRUE;
}

static BOOL TransferControlToHandler(HANDLE process_handle,
                                     HANDLE thread_handle,
                                     DWORD base,
                                     CONTEXT *context,
                                     DWORD exception_handler,
                                     DWORD exception_stack) {
  struct ExceptionFrame new_stack;

  new_stack.return_addr = 0;
  new_stack.prog_ctr = context->Eip;
  new_stack.stack_ptr = context->Esp;
  if (!WriteProcessMemoryChecked(
           process_handle,
           (LPVOID) (base + exception_stack),
           &new_stack,
           sizeof(new_stack))) {
    return FALSE;
  }

  context->Eip = exception_handler;
  context->Esp = exception_stack;
  context->EFlags &= ~NACL_X86_DIRECTION_FLAG;
  return SetThreadContext(thread_handle, context);
}

static BOOL HandleException(HANDLE process_handle, HANDLE thread_handle) {
  CONTEXT context;
  LDT_ENTRY cs_entry;
  LDT_ENTRY ds_entry;
  DWORD base;
  DWORD cs_limit;
  DWORD ds_limit;
  DWORD exception_handler;
  DWORD exception_stack;
  DWORD exception_flag;
  void* nacl_context;

  context.ContextFlags = CONTEXT_SEGMENTS | CONTEXT_INTEGER | CONTEXT_CONTROL;
  if (!GetThreadContext(thread_handle, &context)) {
    return FALSE;
  }
  GetThreadSelectorEntry(thread_handle, context.SegCs, &cs_entry);
  base = cs_entry.BaseLow +
         (((int)cs_entry.HighWord.Bytes.BaseMid) << 16) +
         (((int)cs_entry.HighWord.Bytes.BaseHi) << 24);
  cs_limit = cs_entry.LimitLow + (((int)cs_entry.HighWord.Bits.LimitHi) << 16);
  GetThreadSelectorEntry(thread_handle, context.SegDs, &ds_entry);
  ds_limit = ds_entry.LimitLow + (((int)ds_entry.HighWord.Bits.LimitHi) << 16);
  /* Segment limits are 4Gb in trusted code*/
  if (cs_limit == 0xfffff || ds_limit == 0xfffff) {
    return FALSE;
  }
  if (!GetExceptionHandlingInfo(process_handle, base, &context,
                                &nacl_context, &exception_handler,
                                &exception_stack, &exception_flag)) {
    return FALSE;
  }
  if (exception_flag) {
    return FALSE;
  }

  /*
   * Calculate the position of the exception stack frame, with
   * suitable alignment.
   */
  exception_stack -= sizeof(struct ExceptionFrame) - NACL_STACK_PAD_BELOW_ALIGN;
  exception_stack = exception_stack & ~NACL_STACK_ALIGN_MASK;
  exception_stack -= NACL_STACK_PAD_BELOW_ALIGN;

  if (!IsExceptionHandlerValid(process_handle, base, cs_limit, ds_limit,
                               exception_handler, exception_stack)) {
    return FALSE;
  }
  if (!SetExceptionFlag(process_handle, nacl_context)) {
    return FALSE;
  }
  return TransferControlToHandler(process_handle, thread_handle, base,
                                  &context, exception_handler,
                                  exception_stack);
}

int NaClLaunchAndDebugItself(char *program, DWORD *exit_code) {
  PROCESS_INFORMATION process_information;
  STARTUPINFOA startup_info = {sizeof(STARTUPINFOA)};
  int result;
  if (IsDebuggerPresent()) {
    return DEBUG_EXCEPTION_HANDLER_UNDER_DEBUGGER;
  }
  if (0 == CreateProcessA(
               program,
               GetCommandLineA(),
               NULL,
               NULL,
               TRUE,
               DEBUG_PROCESS,
               NULL,
               NULL,
               &startup_info,
               &process_information)) {
    return DEBUG_EXCEPTION_HANDLER_ERROR;
  }
  CloseHandle(process_information.hThread);
  result = NaClDebugLoop(process_information.hProcess, exit_code);
  CloseHandle(process_information.hProcess);
  return result;
}
