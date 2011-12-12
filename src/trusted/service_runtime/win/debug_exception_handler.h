/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_WIN_DEBUG_EXCEPTION_HANDLER_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_WIN_DEBUG_EXCEPTION_HANDLER_H_
#include <windows.h>

#define DEBUG_EXCEPTION_HANDLER_UNDER_DEBUGGER 0
#define DEBUG_EXCEPTION_HANDLER_SUCCESS 1
#define DEBUG_EXCEPTION_HANDLER_ERROR -1
#define DEBUG_EXCEPTION_HANDLER_NOT_SUPPORTED -2

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/*
 * If under debugger - returns DEBUG_EXCEPTION_HANDLER_UNDER_DEBUGGER.
 * Otherwise, launch itself under debugging and handle exceptions.
 * When child process terminates, returns DEBUG_EXCEPTION_HANDLER_SUCCESS and
 * fills exit_code. In case of error returns DEBUG_EXCEPTION_HANDLER_ERROR.
 * If exception handling with debugger is not supported, returns
 * DEBUG_EXCEPTION_HANDLER_NOT_SUPPORTED.
 */
int NaClLaunchAndDebugItself(char *program, DWORD *exit_code);
/*
 * Runs debug loop until process exits or error occurs.
 * In case of error during debugging returns DEBUG_EXCEPTION_HANDLER_ERROR.
 * On Windows 64-bit returns DEBUG_EXCEPTION_HANDLER_NOT_SUPPORTED.
 * Otherwise returns DEBUG_EXCEPTION_HANDLER_SUCCESS and fills exit_code.
 */
int NaClDebugLoop(HANDLE process_handle, DWORD *exit_code);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* NATIVE_CLIENT_SERVICE_RUNTIME_WIN_DEBUG_EXCEPTION_HANDLER_H_ */