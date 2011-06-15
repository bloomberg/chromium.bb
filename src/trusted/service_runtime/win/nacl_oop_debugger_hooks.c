/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "native_client/src/trusted/service_runtime/nacl_oop_debugger_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

/*
 * Note: this function does not do anything if program
 * is not running under debugger.
 * If program (sel_ldr) is running under debugger, calling this function
 * gives debugger a chance to modify state of the debuggee before
 * resuming debuggee execution.
 */
void SendMessageToDebuggerAndHalt(const char *fmt, ...)
    ATTRIBUTE_FORMAT_PRINTF(1, 2);

void NaClOopDebuggerAppCreateHook(struct NaClApp *nap) {
  if (NULL == nap)
    return;
  SendMessageToDebuggerAndHalt(
      "-event AppCreate -nap %p -mem_start %p -user_entry_pt %p "
      "-initial_entry_pt %p",
      nap,
      (void *) nap->mem_start,
      (void *) nap->user_entry_pt,
      (void *) nap->initial_entry_pt);
}

void NaClOopDebuggerThreadCreateHook(struct NaClAppThread *natp) {
  SendMessageToDebuggerAndHalt("-event ThreadCreate -natp %p", natp);
}

void NaClOopDebuggerThreadExitHook(struct NaClAppThread *natp, int exit_code) {
  SendMessageToDebuggerAndHalt("-event ThreadExit -natp %p -exit_code %d",
                               natp,
                               exit_code);
}

void NaClOopDebuggerAppExitHook(int exit_code) {
  SendMessageToDebuggerAndHalt("-event AppExit -exit_code %d", exit_code);
}

void SendMessageToDebuggerAndHalt(const char *fmt, ...) {
  if (NULL == fmt) {
    NaClLog(LOG_FATAL, "SendMessageToDebuggerAndHalt: fmt == NULL\n");
    return;
  }

#define kVarMsgSize 512
  /*
   * Determines whether the calling process is being debugged by a debugger.
   * IsDebuggerPresent() is a member of Windows Debugging API:
   * http://msdn.microsoft.com/en-us/library/ms680345%28v=VS.85%29.aspx
   */
  if (IsDebuggerPresent()) {
    /*
     * Prefix has GUID string specific to our OOP debugger, so that it
     * can differentiate it from other uses of 'OutputDebugString'.
     */
    char const prefix[] = "{7AA7C9CF-89EC-4ed3-8DAD-6DC84302AB11} -version 1 ";
    char msg[sizeof(prefix) - 1 + kVarMsgSize];
    char *post_pref_msg = msg + sizeof(prefix) - 1;
    signed int res = 0;
    va_list marker;

    strcpy(msg, prefix);
    va_start(marker, fmt);
    res = _vsnprintf_s(post_pref_msg, kVarMsgSize, _TRUNCATE, fmt, marker);
    if (-1 != res) {
      /*
       * Sends a string to the debugger by raising OUTPUT_DEBUG_STRING_EVENT.
       * OutputDebugString() is a member of Windows Debugging API:
       * http://msdn.microsoft.com/en-us/library/aa363362%28v=VS.85%29.aspx
       *
       * It's conceptually eqvivalent to:
       * ::RaiseException(MY_OWN_EXCEPTION_CODE, 0, 1, &msg);
       * The difference is, raising MY_OWN_EXCEPTION_CODE can cause program
       * to crash (if there's no VEH handler and no debugger; or can cause
       * debugger to stop program (if debugger does not know about
       * MY_OWN_EXCEPTION_CODE).
       *
       * On the contrary, using OutputDebugString() is safe, cc from MSDN page:
       * "If the application has no debugger and the system debugger is not
       * active, OutputDebugString does nothing."
       */
      OutputDebugStringA(msg);
    } else {
      NaClLog(LOG_FATAL,
              "SendMessageToDebuggerAndHalt: _vsnprintf_s returned -1\n");
    }
  }
#undef kVarMsgSize
}
/* Example of captured sel_ldr -> debugger messages:

11.62454796 [6060] {7AA7C9CF-89EC-4ed3-8DAD-6DC84302AB11} -version 1 \
  -event AppCreate -nap 00000000001CD3F0 -mem_start 0000000C00000000 \
  -user_entry_pt 0000000000020080 -initial_entry_pt 0000000008000080
11.62480354 [6060] {7AA7C9CF-89EC-4ed3-8DAD-6DC84302AB11} -version 1 \
  -event ThreadCreate -natp 0000000002304C50
...
23.96814728 [6060] {7AA7C9CF-89EC-4ed3-8DAD-6DC84302AB11} -version 1 \
    -event ThreadExit -natp 0000000002304C50 -exit_code 1
23.96838570 [6060] {7AA7C9CF-89EC-4ed3-8DAD-6DC84302AB11} -version 1 \
    -event AppExit -exit_code 1
*/

