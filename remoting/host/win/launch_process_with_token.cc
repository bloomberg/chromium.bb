// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/launch_process_with_token.h"

#include <windows.h>
#include <winternl.h>

#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/scoped_native_library.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"

using base::win::ScopedHandle;

namespace {

const char kCreateProcessDefaultPipeNameFormat[] =
    "\\\\.\\Pipe\\TerminalServer\\SystemExecSrvr\\%d";

// Undocumented WINSTATIONINFOCLASS value causing
// winsta!WinStationQueryInformationW() to return the name of the pipe for
// requesting cross-session process creation.
const WINSTATIONINFOCLASS kCreateProcessPipeNameClass =
    static_cast<WINSTATIONINFOCLASS>(0x21);

const int kPipeBusyWaitTimeoutMs = 2000;
const int kPipeConnectMaxAttempts = 3;

// Terminates the process and closes process and thread handles in
// |process_information| structure.
void CloseHandlesAndTerminateProcess(PROCESS_INFORMATION* process_information) {
  if (process_information->hThread) {
    CloseHandle(process_information->hThread);
    process_information->hThread = NULL;
  }

  if (process_information->hProcess) {
    TerminateProcess(process_information->hProcess, CONTROL_C_EXIT);
    CloseHandle(process_information->hProcess);
    process_information->hProcess = NULL;
  }
}

// Connects to the executor server corresponding to |session_id|.
bool ConnectToExecutionServer(uint32 session_id,
                              base::win::ScopedHandle* pipe_out) {
  base::string16 pipe_name;

  // Use winsta!WinStationQueryInformationW() to determine the process creation
  // pipe name for the session.
  base::FilePath winsta_path(
      base::GetNativeLibraryName(base::UTF8ToUTF16("winsta")));
  base::ScopedNativeLibrary winsta(winsta_path);
  if (winsta.is_valid()) {
    PWINSTATIONQUERYINFORMATIONW win_station_query_information =
        reinterpret_cast<PWINSTATIONQUERYINFORMATIONW>(
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
    pipe_name = base::UTF8ToUTF16(
        base::StringPrintf(kCreateProcessDefaultPipeNameFormat, session_id));
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
    PLOG(ERROR) << "Failed to connect to '" << pipe_name << "'";
    return false;
  }

  *pipe_out = pipe.Pass();
  return true;
}

// Copies the process token making it a primary impersonation token.
// The returned handle will have |desired_access| rights.
bool CopyProcessToken(DWORD desired_access, ScopedHandle* token_out) {
  HANDLE temp_handle;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_DUPLICATE | desired_access,
                        &temp_handle)) {
    PLOG(ERROR) << "Failed to open process token";
    return false;
  }
  ScopedHandle process_token(temp_handle);

  if (!DuplicateTokenEx(process_token,
                        desired_access,
                        NULL,
                        SecurityImpersonation,
                        TokenPrimary,
                        &temp_handle)) {
    PLOG(ERROR) << "Failed to duplicate the process token";
    return false;
  }

  token_out->Set(temp_handle);
  return true;
}

// Creates a copy of the current process with SE_TCB_NAME privilege enabled.
bool CreatePrivilegedToken(ScopedHandle* token_out) {
  ScopedHandle privileged_token;
  DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
                         TOKEN_DUPLICATE | TOKEN_QUERY;
  if (!CopyProcessToken(desired_access, &privileged_token)) {
    return false;
  }

  // Get the LUID for the SE_TCB_NAME privilege.
  TOKEN_PRIVILEGES state;
  state.PrivilegeCount = 1;
  state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  if (!LookupPrivilegeValue(NULL, SE_TCB_NAME, &state.Privileges[0].Luid)) {
    PLOG(ERROR) << "Failed to lookup the LUID for the SE_TCB_NAME privilege";
    return false;
  }

  // Enable the SE_TCB_NAME privilege.
  if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, NULL, 0)) {
    PLOG(ERROR) << "Failed to enable SE_TCB_NAME privilege in a token";
    return false;
  }

  *token_out = privileged_token.Pass();
  return true;
}

// Fills the process and thread handles in the passed |process_information|
// structure and resume the process if the caller didn't want to suspend it.
bool ProcessCreateProcessResponse(DWORD creation_flags,
                                  PROCESS_INFORMATION* process_information) {
  // The execution server does not return handles to the created process and
  // thread.
  if (!process_information->hProcess) {
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
    process_information->hProcess =
        OpenProcess(desired_access,
                    FALSE,
                    process_information->dwProcessId);
    if (!process_information->hProcess) {
      PLOG(ERROR) << "Failed to open the process "
                  << process_information->dwProcessId;
      return false;
    }
  }

  if (!process_information->hThread) {
    // N.B. THREAD_ALL_ACCESS is different in XP and Vista+ versions of
    // the SDK. |desired_access| below is effectively THREAD_ALL_ACCESS from
    // the XP version of the SDK.
    DWORD desired_access =
        STANDARD_RIGHTS_REQUIRED |
        SYNCHRONIZE |
        THREAD_TERMINATE |
        THREAD_SUSPEND_RESUME |
        THREAD_GET_CONTEXT |
        THREAD_SET_CONTEXT |
        THREAD_QUERY_INFORMATION |
        THREAD_SET_INFORMATION |
        THREAD_SET_THREAD_TOKEN |
        THREAD_IMPERSONATE |
        THREAD_DIRECT_IMPERSONATION;
    process_information->hThread =
        OpenThread(desired_access,
                   FALSE,
                   process_information->dwThreadId);
    if (!process_information->hThread) {
      PLOG(ERROR) << "Failed to open the thread "
                  << process_information->dwThreadId;
      return false;
    }
  }

  // Resume the thread if the caller didn't want to suspend the process.
  if ((creation_flags & CREATE_SUSPENDED) == 0) {
    if (!ResumeThread(process_information->hThread)) {
      PLOG(ERROR) << "Failed to resume the thread "
                  << process_information->dwThreadId;
      return false;
    }
  }

  return true;
}

// Receives the response to a remote process create request.
bool ReceiveCreateProcessResponse(
    HANDLE pipe,
    PROCESS_INFORMATION* process_information_out) {
  struct CreateProcessResponse {
    DWORD size;
    BOOL success;
    DWORD last_error;
    PROCESS_INFORMATION process_information;
  };

  DWORD bytes;
  CreateProcessResponse response;
  if (!ReadFile(pipe, &response, sizeof(response), &bytes, NULL)) {
    PLOG(ERROR) << "Failed to receive CreateProcessAsUser response";
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

  *process_information_out = response.process_information;
  return true;
}

// Sends a remote process create request to the execution server.
bool SendCreateProcessRequest(
    HANDLE pipe,
    const base::FilePath::StringType& application_name,
    const base::CommandLine::StringType& command_line,
    DWORD creation_flags,
    const base::char16* desktop_name) {
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

  base::string16 desktop;
  if (desktop_name)
    desktop = desktop_name;

  // Allocate a large enough buffer to hold the CreateProcessRequest structure
  // and three NULL-terminated string parameters.
  size_t size = sizeof(CreateProcessRequest) + sizeof(wchar_t) *
      (application_name.size() + command_line.size() + desktop.size() + 3);
  scoped_ptr<char[]> buffer(new char[size]);
  memset(buffer.get(), 0, size);

  // Marshal the input parameters.
  CreateProcessRequest* request =
      reinterpret_cast<CreateProcessRequest*>(buffer.get());
  request->size = size;
  request->process_id = GetCurrentProcessId();
  request->use_default_token = TRUE;
  // Always pass CREATE_SUSPENDED to avoid a race between the created process
  // exiting too soon and OpenProcess() call below.
  request->creation_flags = creation_flags | CREATE_SUSPENDED;
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
  std::copy(desktop.begin(),
            desktop.end(),
            reinterpret_cast<wchar_t*>(buffer.get() + buffer_offset));

  // Pass the request to create a process in the target session.
  DWORD bytes;
  if (!WriteFile(pipe, buffer.get(), size, &bytes, NULL)) {
    PLOG(ERROR) << "Failed to send CreateProcessAsUser request";
    return false;
  }

  return true;
}

// Requests the execution server to create a process in the specified session
// using the default (i.e. Winlogon) token. This routine relies on undocumented
// OS functionality and will likely not work on anything but XP or W2K3.
bool CreateRemoteSessionProcess(
    uint32 session_id,
    const base::FilePath::StringType& application_name,
    const base::CommandLine::StringType& command_line,
    DWORD creation_flags,
    const base::char16* desktop_name,
    PROCESS_INFORMATION* process_information_out) {
  DCHECK_LT(base::win::GetVersion(), base::win::VERSION_VISTA);

  base::win::ScopedHandle pipe;
  if (!ConnectToExecutionServer(session_id, &pipe))
    return false;

  if (!SendCreateProcessRequest(pipe, application_name, command_line,
                                creation_flags, desktop_name)) {
    return false;
  }

  PROCESS_INFORMATION process_information;
  if (!ReceiveCreateProcessResponse(pipe, &process_information))
    return false;

  if (!ProcessCreateProcessResponse(creation_flags, &process_information)) {
    CloseHandlesAndTerminateProcess(&process_information);
    return false;
  }

  *process_information_out = process_information;
  return true;
}

}  // namespace

namespace remoting {

base::LazyInstance<base::Lock>::Leaky g_inherit_handles_lock =
    LAZY_INSTANCE_INITIALIZER;

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool CreateSessionToken(uint32 session_id, ScopedHandle* token_out) {
  ScopedHandle session_token;
  DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
                         TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;
  if (!CopyProcessToken(desired_access, &session_token)) {
    return false;
  }

  // Temporarily enable the SE_TCB_NAME privilege as it is required by
  // SetTokenInformation(TokenSessionId).
  ScopedHandle privileged_token;
  if (!CreatePrivilegedToken(&privileged_token)) {
    return false;
  }
  if (!ImpersonateLoggedOnUser(privileged_token)) {
    PLOG(ERROR) << "Failed to impersonate the privileged token";
    return false;
  }

  // Change the session ID of the token.
  DWORD new_session_id = session_id;
  if (!SetTokenInformation(session_token,
                           TokenSessionId,
                           &new_session_id,
                           sizeof(new_session_id))) {
    PLOG(ERROR) << "Failed to change session ID of a token";

    // Revert to the default token.
    CHECK(RevertToSelf());
    return false;
  }

  // Revert to the default token.
  CHECK(RevertToSelf());

  *token_out = session_token.Pass();
  return true;
}

bool LaunchProcessWithToken(const base::FilePath& binary,
                            const base::CommandLine::StringType& command_line,
                            HANDLE user_token,
                            SECURITY_ATTRIBUTES* process_attributes,
                            SECURITY_ATTRIBUTES* thread_attributes,
                            bool inherit_handles,
                            DWORD creation_flags,
                            const base::char16* desktop_name,
                            ScopedHandle* process_out,
                            ScopedHandle* thread_out) {
  base::FilePath::StringType application_name = binary.value();

  STARTUPINFOW startup_info;
  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  if (desktop_name)
    startup_info.lpDesktop = const_cast<base::char16*>(desktop_name);

  PROCESS_INFORMATION temp_process_info = {};
  BOOL result = CreateProcessAsUser(user_token,
                                    application_name.c_str(),
                                    const_cast<LPWSTR>(command_line.c_str()),
                                    process_attributes,
                                    thread_attributes,
                                    inherit_handles,
                                    creation_flags,
                                    NULL,
                                    NULL,
                                    &startup_info,
                                    &temp_process_info);

  // CreateProcessAsUser will fail on XP and W2K3 with ERROR_PIPE_NOT_CONNECTED
  // if the user hasn't logged to the target session yet. In such a case
  // we try to talk to the execution server directly emulating what
  // the undocumented and not-exported advapi32!CreateRemoteSessionProcessW()
  // function does. The created process will run under Winlogon'a token instead
  // of |user_token|. Since Winlogon runs as SYSTEM, this suits our needs.
  if (!result &&
      GetLastError() == ERROR_PIPE_NOT_CONNECTED &&
      base::win::GetVersion() < base::win::VERSION_VISTA) {
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
                                          creation_flags,
                                          desktop_name,
                                          &temp_process_info);
    } else {
      // Restore the error status returned by CreateProcessAsUser().
      result = FALSE;
      SetLastError(ERROR_PIPE_NOT_CONNECTED);
    }
  }

  if (!result) {
    PLOG(ERROR) << "Failed to launch a process with a user token";
    return false;
  }

  base::win::ScopedProcessInformation process_info(temp_process_info);

  CHECK(process_info.IsValid());
  process_out->Set(process_info.TakeProcessHandle());
  thread_out->Set(process_info.TakeThreadHandle());
  return true;
}

}  // namespace remoting
