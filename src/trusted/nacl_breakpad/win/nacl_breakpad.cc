/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#include <stdio.h>
#include <map>
#include <string>

// Due to an unfortunate clash in macro definitions, portability.h
// MUST be included first.
#include "native_client/src/include/portability.h"

#include "breakpad/src/client/windows/sender/crash_report_sender.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "native_client/src/trusted/nacl_breakpad/nacl_breakpad.h"
#include "native_client/src/shared/platform/nacl_log.h"

using std::wstring;
using std::map;

using google_breakpad::ExceptionHandler;
using google_breakpad::CrashReportSender;
using google_breakpad::ReportResult;

// Flag to determine whether we attempt to generate dumps for faults
// in untrusted code. Note that the 32-bit Windows version will never
// generate dumps for untrusted code faults, because the OS aborts as
// soon as it sees a value in %ss that it doesn't like.
#ifndef NACL_BREAKPAD_DUMP_UNTRUSTED
#define NACL_BREAKPAD_DUMP_UNTRUSTED 1
#endif

#if NACL_BREAKPAD_DUMP_UNTRUSTED
THREAD ExceptionHandler* g_pBreakpadHandler = NULL;
static volatile LONG handling_exception = 0;
#endif  // NACL_BREAKPAD_DUMP_UNTRUSTED
//
// Please keep these pathnames in sync with those found
// in native_client/tests/breakpad/test_breakpad.py
//
const wchar_t* bpadSubdir = L"Google Native Client Crash Reporting\\";
const wchar_t* dumpSubdir = L"Dumps\\";
const wchar_t* checkpointFilename = L"checkpoint.txt";
wstring dumpPath;
wstring checkpointPath;

static HANDLE hVectoredHandler = INVALID_HANDLE_VALUE;

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

#if NACL_BREAKPAD_DUMP_UNTRUSTED
//
// VectoredHandler: handles exceptions after the first-chance (debugger)
// handler, but long before SEH kicks in.
//
LONG CALLBACK VectoredHandler(PEXCEPTION_POINTERS pExceptionPointers) {
  g_pBreakpadHandler->WriteMinidumpForException(pExceptionPointers);
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
#endif  // NACL_BREAKPAD_DUMP_UNTRUSTED


//
// NaClBreakpadInit: initialize breakpad handlers for nacl
//
void NaClBreakpadInit() {
  const wchar_t kCrashReportURL[] =
    L"http://crash-staging-collector.corp.google.com/cr/report";

  // Get the alternate dump directory. We use the temp path.
  wchar_t tempPathArr[MAX_PATH] = {0};
  ::GetTempPathW(MAX_PATH, tempPathArr);

  wstring tempPath(tempPathArr);
  dumpPath = tempPath + bpadSubdir + dumpSubdir;
  checkpointPath = tempPath + bpadSubdir + checkpointFilename;

  // Ensure that the breakpad dump directories exist
  CreateDirectory((tempPath + bpadSubdir).c_str(), NULL);
  CreateDirectory(dumpPath.c_str(), NULL);

  //
  // See if there's a previous crash dump--if there is, upload it.
  //
  WIN32_FIND_DATA fdat = {0};
  HANDLE hFind = INVALID_HANDLE_VALUE;
  hFind = FindFirstFile((dumpPath + L"*.dmp").c_str(), &fdat);
  if (hFind != INVALID_HANDLE_VALUE) {
    wstring path = dumpPath + fdat.cFileName;
    CrashReportSender sender(checkpointPath.c_str());
    map<wstring, wstring> crashParamMap;

    crashParamMap[L"prod"] = L"Google_Chrome_Native_Client";
    crashParamMap[L"ver"] = L"1.0";
    ReportResult result = sender.SendCrashReport(kCrashReportURL,
                                                 crashParamMap,
                                                 path.c_str(),
                                                 NULL);
    // If the result didn't fail, delete the file. Note that "not failed"
    // does not mean "succeeded" -- the upload may have been rejected or
    // throttled, but in those cases we also don't want to see it again.
    if (result != google_breakpad::RESULT_FAILED) {
      DeleteFile(fdat.cFileName);
    }
  }

  //
  // Clear out old crash dumps
  //
  while (FindNextFile(hFind, &fdat)) {
    DeleteFile(fdat.cFileName);
  }

  FindClose(hFind);

  //
  // Register crash handler
  //

  //
  // The normal breakpad implementaiton uses a structured exception handler.
  // We intercept that by using a vectored exception handler instead. This is
  // important because SEH doesn't work particularly well if the fault
  // is raised from untrusted code.
  //
  // NOTE: Untrusted faults are not caught at all in 32-bit Windows, because
  // the OS exception handler aborts if the value of %ss is not what it
  // expects.
  //
  hVectoredHandler = AddVectoredExceptionHandler(1, VectoredHandler);

  g_pBreakpadHandler = new ExceptionHandler(dumpPath.c_str(),
                                            FilterCallback,
                                            NULL,
                                            NULL,
                                            ExceptionHandler::HANDLER_NONE);
}

//
// NaClBreakpadTeardown: unregister our exception handler and delete the
// breakpad object.
//
void NaClBreakpadFini() {
  RemoveVectoredExceptionHandler(hVectoredHandler);
  delete g_pBreakpadHandler;
}
