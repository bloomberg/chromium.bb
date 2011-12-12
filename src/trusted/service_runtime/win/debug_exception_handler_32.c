/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"
#include "native_client/src/trusted/service_runtime/win/thread_handle_map.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/arch/x86_32/sel_rt_32.h"
#include <windows.h>
#include <stdio.h>

static int HandleBreakpointException(HANDLE thread_handle);
static int HandleException(HANDLE process_handle, HANDLE thread_handle);

typedef DWORD exception_params[3];

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

static BOOL GetAddrProtection(HANDLE process_handle, int addr, int *protect) {
  MEMORY_BASIC_INFORMATION memory_info;
  if (sizeof(memory_info) != VirtualQueryEx(
                                 process_handle,
                                 (LPCVOID)addr,
                                 &memory_info, sizeof(memory_info))) {
    return FALSE;
  }
  *protect = memory_info.Protect;
  return TRUE;
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
  SIZE_T bytes_read;
  int nacl_thread_id = context->SegGs / 8;

  if (!ReadProcessMemory(
           process_handle,
           (LPCVOID)(base + NACL_TRAMPOLINE_END -
                     NACL_SYSCALL_BLOCK_SIZE - sizeof(nacl_user)),
           &nacl_user, sizeof(nacl_user), &bytes_read)) {
    return FALSE;
  }
  if (bytes_read != sizeof(nacl_user)) {
    return FALSE;
  }
  if (!ReadProcessMemory(
           process_handle,
           (LPCVOID)((DWORD_PTR)nacl_user +
                     sizeof(*nacl_context) * nacl_thread_id),
           nacl_context, sizeof(*nacl_context), &bytes_read)) {
    return FALSE;
  }
  if (bytes_read != sizeof(nacl_user)) {
    return FALSE;
  }
  if (!ReadProcessMemory(
           process_handle,
           (LPCVOID)((DWORD_PTR)*nacl_context +
                     NACL_THREAD_CONTEXT_EXCEPTION_HANDLER_OFFSET),
           &handler_data, sizeof(handler_data), &bytes_read)) {
    return FALSE;
  }
  if (bytes_read != sizeof(handler_data)) {
    return FALSE;
  }
  *exception_stack = handler_data[0];
  *exception_flag = handler_data[1];
  if (*exception_stack == 0) {
    *exception_stack = context->Esp;
  }
  if (!ReadProcessMemory(
           process_handle,
           (LPCVOID)(base + NACL_TRAMPOLINE_END - 1 - sizeof(nacl_user) -
                     NACL_SYSCALL_BLOCK_SIZE - sizeof(nap_exception_handler)),
           &nap_exception_handler, sizeof(nap_exception_handler), &bytes_read))
           {
    return FALSE;
  }
  if (bytes_read != sizeof(nap_exception_handler)) {
    return FALSE;
  }
  if (!ReadProcessMemory(
           process_handle,
           (LPCVOID)nap_exception_handler,
           exception_handler, sizeof(*exception_handler), &bytes_read)) {
    return FALSE;
  }
  if (bytes_read != sizeof(*exception_handler)) {
    return FALSE;
  }
  return TRUE;
}

static BOOL SetExceptionFlag(HANDLE process_handle, void* nacl_context) {
  DWORD exception_flag = 1;
  SIZE_T bytes_written;
  if (!WriteProcessMemory(
            process_handle,
            (LPVOID)((DWORD_PTR)nacl_context + 2 * sizeof(DWORD) +
                     NACL_THREAD_CONTEXT_EXCEPTION_HANDLER_OFFSET),
            &exception_flag, sizeof(exception_flag), &bytes_written)) {
    return FALSE;
  }
  if (bytes_written != sizeof(exception_flag)) {
    return FALSE;
  }
  return TRUE;
}

static BOOL IsExceptionHandlerValid(HANDLE process_handle, DWORD base,
                                    DWORD cs_limit, DWORD ds_limit,
                                    DWORD stack_size, DWORD exception_handler,
                                    DWORD exception_stack) {
  int protect;
  if (stack_size > 4096) {
    return FALSE;
  }
  /* check exception_handler value */
  if (exception_handler / 4096 > cs_limit) {
    return FALSE;
  }
  if (exception_handler % NACL_INSTR_BLOCK_SIZE != 0) {
    return FALSE;
  }
  /* check exception_handler rights. */
  if (!GetAddrProtection(process_handle, exception_handler + base, &protect)) {
    return FALSE;
  }
  if ((protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ)) == 0) {
    return FALSE;
  }

  if (exception_stack < stack_size || exception_stack / 4096 > ds_limit) {
    return FALSE;
  }
  /* check exception_stack rights. */
  if (!GetAddrProtection(process_handle, exception_stack - stack_size + base,
                         &protect)) {
    return FALSE;
  }
  if (protect != PAGE_READWRITE) {
    return FALSE;
  }
  if (((exception_stack - 1) / 4096) !=
      ((exception_stack - stack_size) / 4096)) {
    if (!GetAddrProtection(process_handle, exception_stack - 1 + base,
                           &protect)) {
      return FALSE;
    }
    if (protect != PAGE_READWRITE) {
      return FALSE;
    }
  }
  return TRUE;
}

static BOOL TransferControlToHandler(HANDLE process_handle,
                                     HANDLE thread_handle,
                                     DWORD base,
                                     CONTEXT *context,
                                     DWORD exception_handler,
                                     DWORD exception_stack) {
  exception_params new_stack;
  SIZE_T bytes_written;

  new_stack[0] = 0;
  new_stack[1] = context->Eip;
  new_stack[2] = context->Esp;
  if (!WriteProcessMemory(
           process_handle,
           (LPVOID)(exception_stack - sizeof(new_stack) + base),
           &new_stack,
           sizeof(new_stack),
           &bytes_written)) {
    return FALSE;
  }
  if (bytes_written != sizeof(new_stack)) {
    return FALSE;
  }

  context->Eip = exception_handler;
  context->Esp = exception_stack - sizeof(new_stack);
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
  if (!IsExceptionHandlerValid(process_handle, base, cs_limit, ds_limit,
                               sizeof(exception_params), exception_handler,
                               exception_stack)) {
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