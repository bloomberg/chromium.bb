// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_server.h"

#include <windows.h>
#include <wincrypt.h>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/thread.h"
#include "base/utf_string_conversions.h"

#pragma comment(lib, "crypt32.lib")

namespace {

bool LaunchTestServerAsJob(const CommandLine& cmdline,
                           bool start_hidden,
                           base::ProcessHandle* process_handle,
                           ScopedHandle* job_handle) {
  // Launch test server process.
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = start_hidden ? SW_HIDE : SW_SHOW;
  PROCESS_INFORMATION process_info;

  // If this code is run under a debugger, the test server process is
  // automatically associated with a job object created by the debugger.
  // The CREATE_BREAKAWAY_FROM_JOB flag is used to prevent this.
  if (!CreateProcess(
      NULL, const_cast<wchar_t*>(cmdline.command_line_string().c_str()),
      NULL, NULL, TRUE, CREATE_BREAKAWAY_FROM_JOB, NULL, NULL,
      &startup_info, &process_info)) {
    LOG(ERROR) << "Could not create process.";
    return false;
  }
  CloseHandle(process_info.hThread);

  // If the caller wants the process handle, we won't close it.
  if (process_handle) {
    *process_handle = process_info.hProcess;
  } else {
    CloseHandle(process_info.hProcess);
  }

  // Create a JobObject and associate the test server process with it.
  job_handle->Set(CreateJobObject(NULL, NULL));
  if (!job_handle->IsValid()) {
    LOG(ERROR) << "Could not create JobObject.";
    return false;
  } else {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = {0};
    limit_info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (0 == SetInformationJobObject(job_handle->Get(),
      JobObjectExtendedLimitInformation, &limit_info, sizeof(limit_info))) {
      LOG(ERROR) << "Could not SetInformationJobObject.";
      return false;
    }
    if (0 == AssignProcessToJobObject(job_handle->Get(),
                                      process_info.hProcess)) {
      LOG(ERROR) << "Could not AssignProcessToObject.";
      return false;
    }
  }
  return true;
}

void UnblockPipe(HANDLE handle, bool* unblocked) {
  static const char kUnblock[] = "UNBLOCK";
  // Unblock the ReadFile in TestServer::WaitToStart by writing to the pipe.
  // Make sure the call succeeded, otherwise we are very likely to hang.
  DWORD bytes_written = 0;
  CHECK(WriteFile(handle, kUnblock, arraysize(kUnblock), &bytes_written,
                  NULL));
  CHECK_EQ(arraysize(kUnblock), bytes_written);
  *unblocked = true;
}

}  // namespace

namespace net {

bool TestServer::LaunchPython(const FilePath& testserver_path) {
  FilePath python_exe;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &python_exe))
    return false;
  python_exe = python_exe
      .Append(FILE_PATH_LITERAL("third_party"))
      .Append(FILE_PATH_LITERAL("python_24"))
      .Append(FILE_PATH_LITERAL("python.exe"));

  CommandLine python_command(python_exe);
  python_command.AppendArgPath(testserver_path);
  if (!AddCommandLineArguments(&python_command))
    return false;

  HANDLE child_read = NULL;
  HANDLE child_write = NULL;
  if (!CreatePipe(&child_read, &child_write, NULL, 0)) {
    PLOG(ERROR) << "Failed to create pipe";
    return false;
  }
  child_read_fd_.Set(child_read);
  child_write_fd_.Set(child_write);

  // Have the child inherit the write half.
  if (!SetHandleInformation(child_write, HANDLE_FLAG_INHERIT,
                            HANDLE_FLAG_INHERIT)) {
    PLOG(ERROR) << "Failed to enable pipe inheritance";
    return false;
  }

  // Pass the handle on the command-line. Although HANDLE is a
  // pointer, truncating it on 64-bit machines is okay. See
  // http://msdn.microsoft.com/en-us/library/aa384203.aspx
  //
  // "64-bit versions of Windows use 32-bit handles for
  // interoperability. When sharing a handle between 32-bit and 64-bit
  // applications, only the lower 32 bits are significant, so it is
  // safe to truncate the handle (when passing it from 64-bit to
  // 32-bit) or sign-extend the handle (when passing it from 32-bit to
  // 64-bit)."
  python_command.AppendSwitchASCII(
      "startup-pipe",
      base::IntToString(reinterpret_cast<uintptr_t>(child_write)));

  if (!LaunchTestServerAsJob(python_command,
                             true,
                             &process_handle_,
                             &job_handle_)) {
    LOG(ERROR) << "Failed to launch " << python_command.command_line_string();
    return false;
  }

  return true;
}

bool TestServer::WaitToStart() {
  base::Thread thread("test_server_watcher");
  if (!thread.Start())
    return false;

  // Prepare a timeout in case the server fails to start.
  bool unblocked = false;
  thread.message_loop()->PostDelayedTask(FROM_HERE,
      NewRunnableFunction(UnblockPipe, child_write_fd_.Get(), &unblocked),
      TestTimeouts::action_max_timeout_ms());

  char buf[8];
  DWORD bytes_read;
  BOOL result = ReadFile(child_read_fd_.Get(), buf, sizeof(buf), &bytes_read,
                         NULL);

  thread.Stop();
  child_read_fd_.Close();
  child_write_fd_.Close();

  // If we hit the timeout, fail.
  if (unblocked)
    return false;

  return result && bytes_read > 0;
}

bool TestServer::CheckCATrusted() {
  HCERTSTORE cert_store = CertOpenSystemStore(NULL, L"ROOT");
  if (!cert_store) {
    LOG(ERROR) << " could not open trusted root CA store";
    return false;
  }
  PCCERT_CONTEXT cert =
      CertFindCertificateInStore(cert_store,
                                 X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                 0,
                                 CERT_FIND_ISSUER_STR,
                                 L"Test CA",
                                 NULL);
  if (cert)
    CertFreeCertificateContext(cert);
  CertCloseStore(cert_store, 0);

  if (!cert) {
    LOG(ERROR) << " TEST CONFIGURATION ERROR: you need to import the test ca "
                  "certificate to your trusted roots for this test to work. "
                  "For more info visit:\n"
                  "http://dev.chromium.org/developers/testing\n";
    return false;
  }

  return true;
}

}  // namespace net
