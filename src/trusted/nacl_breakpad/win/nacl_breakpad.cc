/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#include <stdio.h>

// Due to an unfortunate clash in macro definitions, portability.h
// MUST be included first.
#include "native_client/src/include/portability.h"


#include "native_client/src/third_party/breakpad/src/client/windows/handler/exception_handler.h"
#include "native_client/src/trusted/nacl_breakpad/nacl_breakpad.h"

using google_breakpad::ExceptionHandler;

THREAD ExceptionHandler* g_pBreakpadHandler = NULL;
static volatile LONG handling_exception = 0;

// FilterCallback copied from Chrome main source
// This callback is executed when the process has crashed and *before*
// the crash dump is created. To prevent duplicate crash reports we
// make every thread calling this method, except the very first one,
// go to sleep.
bool FilterCallback(void* pContext,
                    EXCEPTION_POINTERS* pExceptionPointers,
                    MDRawAssertionInfo* pRaw) {
  UNREFERENCED_PARAMETER(pContext);
  UNREFERENCED_PARAMETER(pExceptionPointers);
  UNREFERENCED_PARAMETER(pRaw);

  // Capture every thread except the first one in the sleep. We don't
  // want multiple threads to concurrently report exceptions.
  if (::InterlockedCompareExchange(&handling_exception, 1, 0) == 1) {
    Sleep(INFINITE);
  }

  // Return true to tell Breakpad that we want a crash dump.
  return true;
}

//
// SecondHandler: handles vectored exceptions after Breakpad has had a crack
// at them.
//
LONG CALLBACK SecondHandler(PEXCEPTION_POINTERS pExceptionPointers) {
  UNREFERENCED_PARAMETER(pExceptionPointers);
  // At this point we do not want control to pass back to the OS exception
  // handling routine, because if the exception came from untrusted code, it
  // will make the OS very confused. We've already written out our crashdump,
  // and we (on purpose) don't have any strategy for continuing untrusted
  // execution after an exception, so the safest thing to do is die, and
  // die quickly.
  //
  // TODO(ilewis): Add code to determine whether the exception was raised in
  // trusted code. If it was, we may want to handle it differently.
  exit(-1);
}

void NaClBreakpadInit() {
  // Get the alternate dump directory. We use the temp path.
  wchar_t temp_dir[MAX_PATH] = {0};
  wchar_t dump_dir[MAX_PATH] = {0};
  ::GetTempPathW(MAX_PATH, temp_dir);
  _snwprintf(dump_dir, MAX_PATH, L"%s\\NaCl_Dumps", temp_dir);
  dump_dir[MAX_PATH-1]=0;

  // Our vectored handlers will be called in LIFO order, because we're
  // asking the OS to add them to the front of the VEH list. So the "second"
  // handler needs to be added first.
  AddVectoredExceptionHandler(1, SecondHandler);

  g_pBreakpadHandler = new ExceptionHandler(dump_dir,
                                            FilterCallback,
                                            NULL,
                                            NULL,
                                            ExceptionHandler::HANDLER_ALL);
}

void NaClBreakpadTeardown() {
  delete g_pBreakpadHandler;
}

