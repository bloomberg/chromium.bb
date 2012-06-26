// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/launch_process_in_session_win.h"

#include <windows.h>
#include <winternl.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_native_library.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"

using base::win::ScopedHandle;

namespace {

const wchar_t kCreateProcessDefaultPipeNameFormat[] =
    L"\\\\.\\Pipe\\TerminalServer\\SystemExecSrvr\\%d";

// Undocumented WINSTATIONINFOCLASS value causing
// winsta!WinStationQueryInformationW() to return the name of the pipe for
// requesting cross-session process creation.
const WINSTATIONINFOCLASS kCreateProcessPipeNameClass =
    static_cast<WINSTATIONINFOCLASS>(0x21);

const int kPipeBusyWaitTimeoutMs = 2000;
const int kPipeConnectMaxAttempts = 3;

// The minimum and maximum delays between attempts to inject host process into
// a session.
const int kMaxLaunchDelaySeconds = 60;
const int kMinLaunchDelaySeconds = 1;

// Name of the default session desktop.
wchar_t kDefaultDesktopName[] = L"winsta0\\default";

// Requests the execution server to create a process in the specified session
// using the default (i.e. Winlogon) token. This routine relies on undocumented
// OS functionality and will likely not work on anything but XP or W2K3.
bool CreateRemoteSessionProcess(
    uint32 session_id,
    const std::wstring& application_name,
    const std::wstring& command_line,
    PROCESS_INFORMATION* process_information_out)
{
  DCHECK(base::win::GetVersion() == base::win::VERSION_XP);

  std::wstring pipe_name;

  // Use winsta!WinStationQueryInformationW() to determine the process creation
  // pipe name for the session.
  FilePath winsta_path(base::GetNativeLibraryName(UTF8ToUTF16("winsta")));
  base::ScopedNativeLibrary winsta(winsta_path);
  if (winsta.is_valid()) {
    PWINSTATIONQUERYINFORMATIONW win_station_query_information =
        static_cast<PWINSTATIONQUERYINFORMATIONW>(
            winsta.GetFunctionPointer("WinStationQueryInformationW"));
    if (win_station_query_information) {
      wchar_t name[MAX_PATH];
      ULONG name_length;
      if (win_station_query_information(0,
                                        session_id,
                                        kCreateProcessPipeNameClass,
                                        name,
                                        sizeof(name),
                                        &name_length)) {
        pipe_name.assign(name);
      }
    }
  }

  // Use the default pipe name if we couldn't query its name.
  if (pipe_name.empty()) {
    pipe_name = StringPrintf(kCreateProcessDefaultPipeNameFormat, session_id);
  }

  // Try to connect to the named pipe.
  base::win::ScopedHandle pipe;
  for (int i = 0; i < kPipeConnectMaxAttempts; ++i) {
    pipe.Set(CreateFile(pipe_name.c_str(),
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        0,
                        NULL));
    if (pipe.IsValid()) {
      break;
    }

    // Cannot continue retrying if error is something other than
    // ERROR_PIPE_BUSY.
    if (GetLastError() != ERROR_PIPE_BUSY) {
      break;
    }

    // Cannot continue retrying if wait on pipe fails.
    if (!WaitNamedPipe(pipe_name.c_str(), kPipeBusyWaitTimeoutMs)) {
      break;
    }
  }

  if (!pipe.IsValid()) {
    LOG_GETLASTERROR(ERROR) << "Failed to connect to '" << pipe_name << "'";
    return false;
  }

  std::wstring desktop_name(kDefaultDesktopName);

  // |CreateProcessRequest| structure passes the same parameters to
  // the execution server as CreateProcessAsUser() function does. Strings are
  // stored as wide strings immediately after the structure. String pointers are
  // represented as byte offsets to string data from the beginning of
  // the structure.
  struct CreateProcessRequest {
    DWORD size;
    DWORD process_id;
    BOOL use_default_token;
    HANDLE token;
    LPWSTR application_name;
    LPWSTR command_line;
    SECURITY_ATTRIBUTES process_attributes;
    SECURITY_ATTRIBUTES thread_attributes;
    BOOL inherit_handles;
    DWORD creation_flags;
    LPVOID environment;
    LPWSTR current_directory;
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_information;
  };

  // Allocate a large enough buffer to hold the CreateProcessRequest structure
  // and three NULL-terminated string parameters.
  size_t size = sizeof(CreateProcessRequest) + sizeof(wchar_t) *
      (application_name.size() + command_line.size() + desktop_name.size() + 3);
  scoped_array<char> buffer(new char[size]);
  memset(buffer.get(), 0, size);

  // Marshal the input parameters.
  CreateProcessRequest* request =
      reinterpret_cast<CreateProcessRequest*>(buffer.get());
  request->size = size;
  request->use_default_token = TRUE;
  request->process_id = GetCurrentProcessId();
  request->startup_info.cb = sizeof(request->startup_info);

  size_t buffer_offset = sizeof(CreateProcessRequest);

  request->application_name = reinterpret_cast<LPWSTR>(buffer_offset);
  std::copy(application_name.begin(),
            application_name.end(),
            reinterpret_cast<wchar_t*>(buffer.get() + buffer_offset));
  buffer_offset += (application_name.size() + 1) * sizeof(wchar_t);

  request->command_line = reinterpret_cast<LPWSTR>(buffer_offset);
  std::copy(command_line.begin(),
            command_line.end(),
            reinterpret_cast<wchar_t*>(buffer.get() + buffer_offset));
  buffer_offset += (command_line.size() + 1) * sizeof(wchar_t);

  request->startup_info.lpDesktop =
      reinterpret_cast<LPWSTR>(buffer_offset);
  std::copy(desktop_name.begin(),
            desktop_name.end(),
            reinterpret_cast<wchar_t*>(buffer.get() + buffer_offset));

  // Pass the request to create a process in the target session.
  DWORD bytes;
  if (!WriteFile(pipe.Get(), buffer.get(), size, &bytes, NULL)) {
    LOG_GETLASTERROR(ERROR) << "Failed to send CreateProcessAsUser request";
    return false;
  }

  // Receive the response.
  struct CreateProcessResponse {
    DWORD size;
    BOOL success;
    DWORD last_error;
    PROCESS_INFORMATION process_information;
  };

  CreateProcessResponse response;
  if (!ReadFile(pipe.Get(), &response, sizeof(response), &bytes, NULL)) {
    LOG_GETLASTERROR(ERROR) << "Failed to receive CreateProcessAsUser response";
    return false;
  }

  // The server sends the data in one chunk so if we didn't received a complete
  // answer something bad happend and there is no point in retrying.
  if (bytes != sizeof(response)) {
    SetLastError(ERROR_RECEIVE_PARTIAL);
    return false;
  }

  if (!response.success) {
    SetLastError(response.last_error);
    return false;
  }

  // The execution server does not return handles to the created process and
  // thread.
  if (response.process_information.hProcess == NULL) {
    // N.B. PROCESS_ALL_ACCESS is different in XP and Vista+ versions of
    // the SDK. |desired_access| below is effectively PROCESS_ALL_ACCESS from
    // the XP version of the SDK.
    DWORD desired_access =
        STANDARD_RIGHTS_REQUIRED |
        SYNCHRONIZE |
        PROCESS_TERMINATE |
        PROCESS_CREATE_THREAD |
        PROCESS_SET_SESSIONID |
        PROCESS_VM_OPERATION |
        PROCESS_VM_READ |
        PROCESS_VM_WRITE |
        PROCESS_DUP_HANDLE |
        PROCESS_CREATE_PROCESS |
        PROCESS_SET_QUOTA |
        PROCESS_SET_INFORMATION |
        PROCESS_QUERY_INFORMATION |
        PROCESS_SUSPEND_RESUME;
    response.process_information.hProcess =
        OpenProcess(desired_access,
                    FALSE,
                    response.process_information.dwProcessId);
    if (!response.process_information.hProcess) {
      LOG_GETLASTERROR(ERROR) << "Failed to open process "
                              << response.process_information.dwProcessId;
      return false;
    }
  }

  *process_information_out = response.process_information;
  return true;
}

} // namespace

namespace remoting {

// Launches |binary| in a different session. The target session is specified by
// |user_token|.
bool LaunchProcessInSession(const FilePath& binary,
                            const std::wstring& command_line,
                            HANDLE user_token,
                            base::Process* process_out) {
  std::wstring application_name = binary.value();

  base::win::ScopedProcessInformation process_info;
  STARTUPINFOW startup_info;

  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  startup_info.lpDesktop = kDefaultDesktopName;

  BOOL result = CreateProcessAsUser(user_token,
                                    application_name.c_str(),
                                    const_cast<LPWSTR>(command_line.c_str()),
                                    NULL,
                                    NULL,
                                    FALSE,
                                    0,
                                    NULL,
                                    NULL,
                                    &startup_info,
                                    process_info.Receive());

  // CreateProcessAsUser will fail on XP and W2K3 with ERROR_PIPE_NOT_CONNECTED
  // if the user hasn't logged to the target session yet. In such a case
  // we try to talk to the execution server directly emulating what
  // the undocumented and not-exported advapi32!CreateRemoteSessionProcessW()
  // function does. The created process will run under Winlogon'a token instead
  // of |user_token|. Since Winlogon runs as SYSTEM, this suits our needs.
  if (!result &&
      GetLastError() == ERROR_PIPE_NOT_CONNECTED &&
      base::win::GetVersion() == base::win::VERSION_XP) {
    DWORD session_id;
    DWORD return_length;
    result = GetTokenInformation(user_token,
                                 TokenSessionId,
                                 &session_id,
                                 sizeof(session_id),
                                 &return_length);
    if (result && session_id != 0) {
      result = CreateRemoteSessionProcess(session_id,
                                          application_name,
                                          command_line,
                                          process_info.Receive());
    } else {
      // Restore the error status returned by CreateProcessAsUser().
      result = FALSE;
      SetLastError(ERROR_PIPE_NOT_CONNECTED);
    }
  }

  if (!result) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to launch a process with a user token";
    return false;
  }

  CHECK(process_info.IsValid());
  process_out->set_handle(process_info.TakeProcessHandle());
  return true;
}

} // namespace remoting
