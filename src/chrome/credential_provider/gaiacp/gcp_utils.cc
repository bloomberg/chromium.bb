// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcp_utils.h"

#include <windows.h>
#include <winternl.h>
#include <wincred.h>  // For <ntsecapi.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <MDMRegistration.h>  // For RegisterDeviceWithManagement()
#include <ntsecapi.h>         // For LsaLookupAuthenticationPackage()
#include <sddl.h>             // For ConvertSidToStringSid()
#include <security.h>         // For NEGOSSP_NAME_A

#include <atlconv.h>

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>

#include <iomanip>
#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/credential_provider/gaiacp/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

namespace {

HRESULT RegisterWithGoogleDeviceManagement(const base::string16& mdm_url,
                                           const base::string16& email,
                                           const base::string16& token) {
  base::ScopedNativeLibrary library(
      base::FilePath(FILE_PATH_LITERAL("MDMRegistration.dll")));
  if (!library.is_valid()) {
    LOGFN(ERROR) << "base::ScopedNativeLibrary hr=" << putHR(E_NOTIMPL);
    return E_NOTIMPL;
  }

  using RegisterDeviceWithManagementFunction =
      decltype(&::RegisterDeviceWithManagement);
  RegisterDeviceWithManagementFunction
      register_device_with_management_function =
          reinterpret_cast<RegisterDeviceWithManagementFunction>(
              library.GetFunctionPointer("RegisterDeviceWithManagement"));
  if (!register_device_with_management_function) {
    LOGFN(ERROR) << "library.GetFunctionPointer hr=" << putHR(E_NOTIMPL);
    return E_NOTIMPL;
  }

  return register_device_with_management_function(
      email.c_str(), mdm_url.c_str(), token.c_str());
}

}  // namespace

// StdParentHandles ///////////////////////////////////////////////////////////

StdParentHandles::StdParentHandles() {}

StdParentHandles::~StdParentHandles() {}

// ScopedStartupInfo //////////////////////////////////////////////////////////

ScopedStartupInfo::ScopedStartupInfo() {
  memset(&info_, 0, sizeof(info_));
  info_.hStdInput = INVALID_HANDLE_VALUE;
  info_.hStdOutput = INVALID_HANDLE_VALUE;
  info_.hStdError = INVALID_HANDLE_VALUE;
  info_.cb = sizeof(info_);
}

ScopedStartupInfo::ScopedStartupInfo(const wchar_t* desktop)
    : ScopedStartupInfo() {
  DCHECK(desktop);
  desktop_.assign(desktop);
  info_.lpDesktop = const_cast<wchar_t*>(desktop_.c_str());
}

ScopedStartupInfo::~ScopedStartupInfo() {
  Shutdown();
}

HRESULT ScopedStartupInfo::SetStdHandles(
    base::win::ScopedHandle* hstdin,
    base::win::ScopedHandle* hstdout,
    base::win::ScopedHandle* hstderr) {
  if ((info_.dwFlags & STARTF_USESTDHANDLES) == STARTF_USESTDHANDLES) {
    LOGFN(ERROR) << "Already set";
    return E_UNEXPECTED;
  }

  info_.dwFlags |= STARTF_USESTDHANDLES;
  info_.hStdInput = hstdin->Take();
  info_.hStdOutput = hstdout->Take();
  info_.hStdError = hstderr->Take();
  return S_OK;
}

void ScopedStartupInfo::Shutdown() {
  if ((info_.dwFlags & STARTF_USESTDHANDLES) == STARTF_USESTDHANDLES) {
    info_.dwFlags &= ~STARTF_USESTDHANDLES;

    ::CloseHandle(info_.hStdInput);
    ::CloseHandle(info_.hStdOutput);
    ::CloseHandle(info_.hStdError);
    info_.hStdInput = INVALID_HANDLE_VALUE;
    info_.hStdOutput = INVALID_HANDLE_VALUE;
    info_.hStdError = INVALID_HANDLE_VALUE;
  }
}

// Waits for a process to terminate while capturing its stdout and stderr to
// the buffers |stdout_buffer| and |stderr_buffer| respectively.  Both buffers
// are assumed to be of the size |buffer_size| and expected to be relatively
// small.  The exit code of the process is written to |exit_code|.
HRESULT WaitForProcess(base::win::ScopedHandle::Handle process_handle,
                       const StdParentHandles& parent_handles,
                       DWORD* exit_code,
                       char* stdout_buffer,
                       char* stderr_buffer,
                       int buffer_size) {
  LOGFN(INFO);
  DCHECK(exit_code);
  DCHECK_GT(buffer_size, 0);

  stdout_buffer[0] = 0;
  stderr_buffer[0] = 0;

  // stdio handles to wait on.  This array and count are modified by code
  // below if errors are detected.
  base::win::ScopedHandle::Handle handles[] = {
      parent_handles.hstdout_read.Get(), parent_handles.hstderr_read.Get(),
  };
  DWORD count = base::size(handles);

  for (bool is_done = false; !is_done && count > 0;) {
    base::win::ScopedHandle::Handle h = INVALID_HANDLE_VALUE;
    char buffer[80];
    DWORD length = base::size(buffer) - 1;
    HRESULT hr = S_OK;

    const DWORD kThreeMinutesInMs = 3 * 60 * 1000;
    DWORD ret = ::WaitForMultipleObjectsEx(count, handles,
                                           FALSE,              // wait all
                                           kThreeMinutesInMs,  // timeout ms
                                           TRUE);              // alertable wait
    switch (ret) {
      case WAIT_OBJECT_0:
      case WAIT_OBJECT_0 + 1: {
        int index = ret - WAIT_OBJECT_0;
        LOGFN(INFO) << "WAIT_OBJECT_" << index;
        h = handles[index];
        if (!::ReadFile(h, buffer, length, &length, nullptr)) {
          hr = HRESULT_FROM_WIN32(::GetLastError());
          if (hr != HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE))
            LOGFN(ERROR) << "ReadFile(" << index << ") hr=" << putHR(hr);
        } else {
          LOGFN(INFO) << "ReadFile(" << index << ") length=" << length;
          buffer[length] = 0;
        }
        break;
      }
      case WAIT_IO_COMPLETION:
        // This is normal.  Just ignore.
        LOGFN(INFO) << "WaitForMultipleObjectsEx WAIT_IO_COMPLETION";
        break;
      case WAIT_TIMEOUT: {
        // User took too long to log in, so kill UI process.
        LOGFN(INFO) << "WaitForMultipleObjectsEx WAIT_TIMEOUT, killing UI";
        ::TerminateProcess(process_handle, kUiecTimeout);
        is_done = true;
        break;
      }
      case WAIT_FAILED:
      default: {
        HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "WaitForMultipleObjectsEx hr=" << putHR(hr);
        is_done = true;
        break;
      }
    }

    // Copy the read buffer to either the stdout or stderr, as apppropriate.
    // If the pipe was broken, remove the corresponding handle from |handles|
    // so that WaitForMultipleObjectsEx() above no longer waits for it.
    if (h == parent_handles.hstdout_read.Get()) {
      if (hr == HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE)) {
        LOGFN(INFO) << "Stop waiting for stdout";
        handles[0] = handles[1];
        --count;
      } else {
        strcat_s(stdout_buffer, buffer_size, buffer);
      }
    } else if (h == parent_handles.hstderr_read.Get()) {
      if (hr == HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE)) {
        LOGFN(INFO) << "Stop waiting for stderr";
        --count;
      } else {
        strcat_s(stderr_buffer, buffer_size, buffer);
      }
    }
  }

  // At this point both stdout and stderr have been closed.  Wait on the process
  // handle for the process to terminate, getting the exit code.  If the
  // process does not terminate gracefully, kill it before returning.
  DWORD ret = ::WaitForSingleObject(process_handle, 10000);
  if (ret == 0) {
    if (::GetExitCodeProcess(process_handle, exit_code)) {
      LOGFN(INFO) << "Process terminated with exit code " << *exit_code;
    } else {
      LOGFN(INFO) << "Process terminated without exit code";
      *exit_code = kUiecAbort;
    }
  } else {
    LOGFN(INFO) << "UI did not terminiate within 10 seconds, killing now";
    ::TerminateProcess(process_handle, kUiecKilled);
    *exit_code = kUiecKilled;
  }

  return S_OK;
}

HRESULT CreateLogonToken(const wchar_t* username,
                         const wchar_t* password,
                         base::win::ScopedHandle* token) {
  DCHECK(username);
  DCHECK(password);
  DCHECK(token);

  base::win::ScopedHandle::Handle handle;
  if (!::LogonUserW(username, L".", password, LOGON32_LOGON_BATCH,
                    LOGON32_PROVIDER_DEFAULT, &handle)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "LogonUserW hr=" << putHR(hr);
    return hr;
  }
  base::win::ScopedHandle primary_token(handle);

  if (!::CreateRestrictedToken(primary_token.Get(), DISABLE_MAX_PRIVILEGE, 0,
                               nullptr, 0, nullptr, 0, nullptr, &handle)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CreateRestrictedToken hr=" << putHR(hr);
    return hr;
  }
  token->Set(handle);
  return S_OK;
}

HRESULT CreateJobForSignin(base::win::ScopedHandle* job) {
  LOGFN(INFO);
  DCHECK(job);

  job->Set(::CreateJobObject(nullptr, nullptr));
  if (!job->IsValid()) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CreateJobObject hr=" << putHR(hr);
    return hr;
  }

  JOBOBJECT_BASIC_UI_RESTRICTIONS ui;
  ui.UIRestrictionsClass =
      JOB_OBJECT_UILIMIT_DESKTOP |           // Create/switch desktops.
      JOB_OBJECT_UILIMIT_HANDLES |           // Only access own handles.
      JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |  // Cannot set sys params.
      JOB_OBJECT_UILIMIT_WRITECLIPBOARD;     // Cannot write to clipboard.
  if (!::SetInformationJobObject(job->Get(), JobObjectBasicUIRestrictions, &ui,
                                 sizeof(ui))) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "SetInformationJobObject hr=" << putHR(hr);
    return hr;
  }

  return S_OK;
}

HRESULT CreatePipeForChildProcess(bool child_reads,
                                  bool use_nul,
                                  base::win::ScopedHandle* reading,
                                  base::win::ScopedHandle* writing) {
  // Make sure that all handles created here are inheritable.  It is important
  // that the child side handle is inherited.
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = nullptr;

  if (use_nul) {
    base::win::ScopedHandle h(
        ::CreateFileW(L"nul:", FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                      FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                      &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!h.IsValid()) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "CreateFile(nul) hr=" << putHR(hr);
      return hr;
    }

    if (child_reads) {
      reading->Set(h.Take());
    } else {
      writing->Set(h.Take());
    }
  } else {
    base::win::ScopedHandle::Handle temp_handle1;
    base::win::ScopedHandle::Handle temp_handle2;
    if (!::CreatePipe(&temp_handle1, &temp_handle2, &sa, 0)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "CreatePipe(reading) hr=" << putHR(hr);
      return hr;
    }
    reading->Set(temp_handle1);
    writing->Set(temp_handle2);

    // Make sure parent side is not inherited.
    if (!::SetHandleInformation(child_reads ? writing->Get() : reading->Get(),
                                HANDLE_FLAG_INHERIT, 0)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "SetHandleInformation(parent) hr=" << putHR(hr);
      return hr;
    }
  }

  return S_OK;
}

HRESULT InitializeStdHandles(CommDirection direction,
                             ScopedStartupInfo* startupinfo,
                             StdParentHandles* parent_handles) {
  LOGFN(INFO);
  DCHECK(startupinfo);
  DCHECK(parent_handles);

  base::win::ScopedHandle hstdin_read;
  base::win::ScopedHandle hstdin_write;
  HRESULT hr = CreatePipeForChildProcess(
      true,                                            // child reads
      direction == CommDirection::kChildToParentOnly,  // use nul
      &hstdin_read, &hstdin_write);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreatePipeForChildProcess(stdin) hr=" << putHR(hr);
    return hr;
  }

  base::win::ScopedHandle hstdout_read;
  base::win::ScopedHandle hstdout_write;
  hr = CreatePipeForChildProcess(
      false,                                           // child reads
      direction == CommDirection::kParentToChildOnly,  // use nul
      &hstdout_read, &hstdout_write);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreatePipeForChildProcess(stdout) hr=" << putHR(hr);
    return hr;
  }

  base::win::ScopedHandle hstderr_read;
  base::win::ScopedHandle hstderr_write;
  hr = CreatePipeForChildProcess(
      false,                                           // child reads
      direction == CommDirection::kParentToChildOnly,  // use nul
      &hstderr_read, &hstderr_write);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreatePipeForChildProcess(stderr) hr=" << putHR(hr);
    return hr;
  }

  hr = startupinfo->SetStdHandles(&hstdin_read, &hstdout_write, &hstderr_write);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "startupinfo->SetStdHandles hr=" << putHR(hr);
    return hr;
  }

  parent_handles->hstdin_write.Set(hstdin_write.Take());
  parent_handles->hstdout_read.Set(hstdout_read.Take());
  parent_handles->hstderr_read.Set(hstderr_read.Take());
  return S_OK;
}

HRESULT GetCommandLineForEntrypoint(HINSTANCE hDll,
                                    const wchar_t* entrypoint,
                                    wchar_t* command_line,
                                    size_t command_line_length) {
  DCHECK(entrypoint);
  DCHECK(command_line);

  // rundll32 expects the first command line argument to be the path to the
  // DLL, followed by a comma and the name of the function to call.  There can
  // be no spaces around the comma.  There can be no spaces in the path.  It
  // is recommended to use the short path name of the DLL.
  wchar_t path[MAX_PATH];
  DWORD length = base::size(path);
  length = ::GetModuleFileName(hDll, path, length);
  if (length == base::size(path) &&
      ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetModuleFileNameW hr=" << putHR(hr);
    return hr;
  }

  wchar_t short_path[MAX_PATH];
  DWORD short_length = base::size(short_path);
  short_length = ::GetShortPathName(path, short_path, short_length);
  if (short_length >= base::size(short_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetShortPathNameW hr=" << putHR(hr);
    return hr;
  }

  const wchar_t kCommandLineFormat[] = L"rundll32 %s,%s";
  const int kCommandLineFormatRequiredLength = 10;

  // The command line buffer needs to be at least as large as the short path
  // name plus the entrypoint length plus the extra overhead of the command
  // line formatting.
  if (command_line_length < short_length + wcslen(entrypoint) +
      kCommandLineFormatRequiredLength) {
    LOGFN(ERROR) << "command_line_length too short";
    return E_OUTOFMEMORY;
  }

  if (swprintf_s(command_line, command_line_length, kCommandLineFormat,
                 short_path, entrypoint) == -1) {
    LOGFN(ERROR) << "_stprintf_s(command_line) doserror=" << _doserrno;
    return E_OUTOFMEMORY;
  }

  return S_OK;
}

HRESULT EnrollToGoogleMdmIfNeeded(const base::DictionaryValue& properties) {
  USES_CONVERSION;
  LOGFN(INFO);

  // Enroll to Google MDM if not already enrolled.

  HRESULT hr = E_FAIL;
  BOOL is_registered = base::win::IsDeviceRegisteredWithManagement();
  LOGFN(INFO) << "MDM is_registered=" << is_registered;

  if (!is_registered) {
    base::string16 email = GetDictString(&properties, kKeyEmail);
    base::string16 token = GetDictString(&properties, kKeyMdmAcessToken);
    base::string16 mdm_url = GetDictString(&properties, kKeyMdmUrl);

    if (email.empty()) {
      LOGFN(ERROR) << "Email is empty";
      return E_INVALIDARG;
    }

    if (token.empty()) {
      LOGFN(ERROR) << "MDM access token is empty";
      return E_INVALIDARG;
    }

    if (mdm_url.empty()) {
      LOGFN(ERROR) << "No MDM URL specified";
      return E_INVALIDARG;
    }

    LOGFN(INFO) << "MDM_URL=" << mdm_url
                << " token=" << base::string16(token.c_str(), 10);

    hr = RegisterWithGoogleDeviceManagement(mdm_url, email, token);
    LOGFN(INFO) << "RegisterWithGoogleDeviceManagement hr=" << putHR(hr);
  }

  return hr;
}

HRESULT GetAuthenticationPackageId(ULONG* id) {
  DCHECK(id);

  HANDLE lsa;
  NTSTATUS status = ::LsaConnectUntrusted(&lsa);
  HRESULT hr = HRESULT_FROM_NT(status);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "LsaConnectUntrusted hr=" << putHR(hr);
    return hr;
  }

  LSA_STRING name;
  name.Buffer = const_cast<PCHAR>(NEGOSSP_NAME_A);
  name.Length = static_cast<USHORT>(strlen(name.Buffer));
  name.MaximumLength = name.Length + 1;

  status = ::LsaLookupAuthenticationPackage(lsa, &name, id);
  ::LsaDeregisterLogonProcess(lsa);
  hr = HRESULT_FROM_NT(status);
  if (FAILED(hr))
    LOGFN(ERROR) << "LsaLookupAuthenticationPackage hr=" << putHR(hr);

  return hr;
}

base::string16 GetStringResource(UINT id) {
  // When LoadStringW receives 0 as the fourth argument (buffer length), it
  // assumes the third argument (buffer) is a pointer.  The returned pointer
  // is still owned by the system and must not be freed.  Furthermore the string
  // pointed at is not null terminated, so the returned length must be used to
  // construct the final base::string16.
  wchar_t* str;
  int length =
      ::LoadStringW(CURRENT_MODULE(), id, reinterpret_cast<wchar_t*>(&str), 0);
  return base::string16(str, length);
}

base::string16 GetDictString(
    const base::DictionaryValue* dict,
    const char* name) {
  DCHECK(name);
  auto* value = dict->FindKey(name);
  return value && value->is_string() ? base::UTF8ToUTF16(value->GetString())
                                     : base::string16();
}

base::string16 GetDictString(const std::unique_ptr<base::DictionaryValue>& dict,
                             const char* name) {
  return GetDictString(dict.get(), name);
}

std::string GetDictStringUTF8(
    const base::DictionaryValue* dict,
    const char* name) {
  DCHECK(name);
  auto* value = dict->FindKey(name);
  return value && value->is_string() ? value->GetString() : std::string();
}

std::string GetDictStringUTF8(
    const std::unique_ptr<base::DictionaryValue>& dict,
    const char* name) {
  return GetDictStringUTF8(dict.get(), name);
}

FakesForTesting::FakesForTesting() {}

FakesForTesting::~FakesForTesting() {}

}  // namespace credential_provider
