/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// C/C++ library for handle passing in the Windows Chrome sandbox
// browser plugin interface.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_HANDLE_PASS_BROWSER_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_HANDLE_PASS_BROWSER_HANDLE_H_

#if NACL_WINDOWS && !defined(NACL_STANDALONE)

#include <windows.h>
#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"

EXTERN_C_BEGIN

// This function needs to be called once in each renderer process.
// We don't merge it with the Ctor to avoid allocating more memory when it's
// not necessary.
int NaClHandlePassBrowserInit();

// Initializes the maps, etc., maintained by the handle passing library.
extern int NaClHandlePassBrowserCtor();

// Sel_ldr instances will look up PIDs to get HANDLEs by means of an SRPC
// connection.  This method returns the socket address that a sel_ldr instance
// uses to connect to the lookup server.  This will start a thread that
// waits to accept a connection from a sel_ldr.
extern struct NaClDesc* NaClHandlePassBrowserGetSocketAddress();

// Whenever the browser plugin causes a sel_ldr process to be started by
// the browser, it will return the PID and the process HANDLE of the sel_ldr.
// For lookups this pair is remembered by the library.
// NB: It is important to note that the lookup service exported by this
// library will duplicate handles between any two processes remembered by
// it.  If higher capability processes are remembered this way then it may
// be possible to copy higher capability HANDLEs into lower capability
// processes.
extern void NaClHandlePassBrowserRememberHandle(DWORD pid, HANDLE handle);

// Lookup a handle from the renderer process.
extern HANDLE NaClHandlePassBrowserLookupHandle(DWORD pid);

// Destroys this instance of the handle passing service.
extern void NaClHandlePassBrowserDtor();

EXTERN_C_END

#endif  // NACL_WINDOWS && !defined(NACL_STANDALONE)

#endif  // NATIVE_CLIENT_SRC_TRUSTED_HANDLE_PASS_BROWSER_HANDLE_H_
