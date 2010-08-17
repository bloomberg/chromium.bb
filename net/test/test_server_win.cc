// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_server.h"

#include <windows.h>
#include <wincrypt.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

#pragma comment(lib, "crypt32.lib")

namespace {

bool LaunchTestServerAsJob(const std::wstring& cmdline,
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
  if (!CreateProcess(NULL,
                     const_cast<wchar_t*>(cmdline.c_str()), NULL, NULL,
                     FALSE, CREATE_BREAKAWAY_FROM_JOB, NULL, NULL,
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

}  // namespace

namespace net {

// static
void TestServer::AppendToPythonPath(const FilePath& dir) {
  const wchar_t kPythonPath[] = L"PYTHONPATH";
  // TODO(dkegel): handle longer PYTHONPATH variables
  wchar_t oldpath[4096];
  if (GetEnvironmentVariable(kPythonPath, oldpath, arraysize(oldpath)) == 0) {
    SetEnvironmentVariableW(kPythonPath, dir.value().c_str());
  } else if (!wcsstr(oldpath, dir.value().c_str())) {
    std::wstring newpath(oldpath);
    newpath.append(L";");
    newpath.append(dir.value());
    SetEnvironmentVariableW(kPythonPath, newpath.c_str());
  }
}

bool TestServer::LaunchPython(const FilePath& testserver_path) {
  FilePath python_exe;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &python_exe))
    return false;
  python_exe = python_exe
      .Append(FILE_PATH_LITERAL("third_party"))
      .Append(FILE_PATH_LITERAL("python_24"))
      .Append(FILE_PATH_LITERAL("python.exe"));

  std::wstring command_line =
      L"\"" + python_exe.value() + L"\" " +
      L"\"" + testserver_path.value() +
      L"\" --port=" + ASCIIToWide(base::IntToString(host_port_pair_.port())) +
      L" --data-dir=\"" + document_root_.value() + L"\"";

  if (type_ == TYPE_FTP)
    command_line.append(L" -f");

  FilePath certificate_path(GetCertificatePath());
  if (!certificate_path.value().empty()) {
    if (!file_util::PathExists(certificate_path)) {
      LOG(ERROR) << "Certificate path " << certificate_path.value()
                 << " doesn't exist. Can't launch https server.";
      return false;
    }
    command_line.append(L" --https=\"");
    command_line.append(certificate_path.value());
    command_line.append(L"\"");
  }

  if (type_ == TYPE_HTTPS_CLIENT_AUTH)
    command_line.append(L" --ssl-client-auth");

  if (!LaunchTestServerAsJob(command_line,
                             true,
                             &process_handle_,
                             &job_handle_)) {
    LOG(ERROR) << "Failed to launch " << command_line;
    return false;
  }

  return true;
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
