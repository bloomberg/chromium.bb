// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/target_process.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/free_deleter.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/access_token.h"
#include "base/win/security_util.h"
#include "base/win/startup_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/sharedmem_ipc_server.h"
#include "sandbox/win/src/startup_information_helper.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

void CopyPolicyToTarget(const void* source, size_t size, void* dest) {
  if (!source || !size)
    return;
  memcpy(dest, source, size);
  sandbox::PolicyGlobal* policy =
      reinterpret_cast<sandbox::PolicyGlobal*>(dest);

  size_t offset = reinterpret_cast<size_t>(source);

  for (size_t i = 0; i < sandbox::kMaxServiceCount; i++) {
    size_t buffer = reinterpret_cast<size_t>(policy->entry[i]);
    if (buffer) {
      buffer -= offset;
      policy->entry[i] = reinterpret_cast<sandbox::PolicyBuffer*>(buffer);
    }
  }
}

bool GetAppContainerImpersonationToken(
    HANDLE process,
    HANDLE initial_token,
    const std::vector<base::win::Sid>& capabilities,
    base::win::ScopedHandle* impersonation_token) {
  absl::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromProcess(process);
  if (!token)
    return false;
  auto app_container_sid = token->AppContainerSid();
  if (!app_container_sid)
    return false;
  SecurityCapabilities security_caps(*app_container_sid, capabilities);
  return CreateLowBoxToken(initial_token, IMPERSONATION, &security_caps,
                           nullptr, 0, impersonation_token) == ERROR_SUCCESS;
}

}  // namespace

SANDBOX_INTERCEPT HANDLE g_shared_section;
SANDBOX_INTERCEPT size_t g_shared_IPC_size;
SANDBOX_INTERCEPT size_t g_shared_policy_size;

TargetProcess::TargetProcess(
    base::win::ScopedHandle initial_token,
    base::win::ScopedHandle lockdown_token,
    HANDLE job,
    ThreadPool* thread_pool,
    const std::vector<base::win::Sid>& impersonation_capabilities)
    // This object owns everything initialized here except thread_pool and
    // the job_ handle. The Job handle is closed by BrokerServices and results
    // eventually in a call to our dtor.
    : lockdown_token_(std::move(lockdown_token)),
      initial_token_(std::move(initial_token)),
      job_(job),
      thread_pool_(thread_pool),
      base_address_(nullptr),
      impersonation_capabilities_(
          base::win::CloneSidVector(impersonation_capabilities)) {}

TargetProcess::~TargetProcess() {
  // Give a chance to the process to die. In most cases the JOB_KILL_ON_CLOSE
  // will take effect only when the context changes. As far as the testing went,
  // this wait was enough to switch context and kill the processes in the job.
  // If this process is already dead, the function will return without waiting.
  // For now, this wait is there only to do a best effort to prevent some leaks
  // from showing up in purify.
  if (sandbox_process_info_.IsValid()) {
    ::WaitForSingleObject(sandbox_process_info_.process_handle(), 50);
    // Terminate the process if it's still alive, as its IPC server is going
    // away. 1 is RESULT_CODE_KILLED.
    ::TerminateProcess(sandbox_process_info_.process_handle(), 1);
  }

  // ipc_server_ references our process handle, so make sure the former is shut
  // down before the latter is closed (by ScopedProcessInformation).
  ipc_server_.reset();
}

// Creates the target (child) process suspended and assigns it to the job
// object.
ResultCode TargetProcess::Create(
    const wchar_t* exe_path,
    const wchar_t* command_line,
    std::unique_ptr<StartupInformationHelper> startup_info_helper,
    base::win::ScopedProcessInformation* target_info,
    DWORD* win_error) {
  exe_name_.reset(_wcsdup(exe_path));

  base::win::StartupInformation* startup_info =
      startup_info_helper->GetStartupInformation();

  // the command line needs to be writable by CreateProcess().
  std::unique_ptr<wchar_t, base::FreeDeleter> cmd_line(_wcsdup(command_line));

  // Start the target process suspended.
  DWORD flags =
      CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | DETACHED_PROCESS;

  if (startup_info->has_extended_startup_info())
    flags |= EXTENDED_STARTUPINFO_PRESENT;

  if (job_ && base::win::GetVersion() < base::win::Version::WIN8) {
    // Windows 8 implements nested jobs, but for older systems we need to
    // break out of any job we're in to enforce our restrictions.
    flags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  bool inherit_handles = startup_info_helper->ShouldInheritHandles();
  PROCESS_INFORMATION temp_process_info = {};
  if (!::CreateProcessAsUserW(lockdown_token_.Get(), exe_path, cmd_line.get(),
                              nullptr,  // No security attribute.
                              nullptr,  // No thread attribute.
                              inherit_handles, flags,
                              nullptr,  // Use the environment of the caller.
                              nullptr,  // Use current directory of the caller.
                              startup_info->startup_info(),
                              &temp_process_info)) {
    *win_error = ::GetLastError();
    return SBOX_ERROR_CREATE_PROCESS;
  }
  base::win::ScopedProcessInformation process_info(temp_process_info);

  if (job_ && !startup_info_helper->HasJobsToAssociate()) {
    DCHECK(base::win::GetVersion() < base::win::Version::WIN10);
    // Assign the suspended target to the windows job object. On Win 10
    // this happens through PROC_THREAD_ATTRIBUTE_JOB_LIST.
    if (!::AssignProcessToJobObject(job_, process_info.process_handle())) {
      *win_error = ::GetLastError();
      ::TerminateProcess(process_info.process_handle(), 0);
      return SBOX_ERROR_ASSIGN_PROCESS_TO_JOB_OBJECT;
    }
  }

  if (initial_token_.IsValid()) {
    HANDLE impersonation_token = initial_token_.Get();
    base::win::ScopedHandle app_container_token;
    if (GetAppContainerImpersonationToken(
            process_info.process_handle(), impersonation_token,
            impersonation_capabilities_, &app_container_token)) {
      impersonation_token = app_container_token.Get();
    }

    // Change the token of the main thread of the new process for the
    // impersonation token with more rights. This allows the target to start;
    // otherwise it will crash too early for us to help.
    HANDLE temp_thread = process_info.thread_handle();
    if (!::SetThreadToken(&temp_thread, impersonation_token)) {
      *win_error = ::GetLastError();
      ::TerminateProcess(process_info.process_handle(), 0);
      return SBOX_ERROR_SET_THREAD_TOKEN;
    }
    initial_token_.Close();
  }

  if (!target_info->DuplicateFrom(process_info)) {
    *win_error = ::GetLastError();  // This may or may not be correct.
    ::TerminateProcess(process_info.process_handle(), 0);
    return SBOX_ERROR_DUPLICATE_TARGET_INFO;
  }

  base_address_ = GetProcessBaseAddress(process_info.process_handle());
  DCHECK(base_address_);
  if (!base_address_) {
    *win_error = ::GetLastError();
    ::TerminateProcess(process_info.process_handle(), 0);
    return SBOX_ERROR_CANNOT_FIND_BASE_ADDRESS;
  }

  sandbox_process_info_.Set(process_info.Take());
  return SBOX_ALL_OK;
}

ResultCode TargetProcess::TransferVariable(const char* name,
                                           void* address,
                                           size_t size) {
  if (!sandbox_process_info_.IsValid())
    return SBOX_ERROR_UNEXPECTED_CALL;

  SIZE_T written;
  if (!::WriteProcessMemory(sandbox_process_info_.process_handle(), address,
                            address, size, &written))
    return SBOX_ERROR_CANNOT_WRITE_VARIABLE_VALUE;

  if (written != size)
    return SBOX_ERROR_INVALID_WRITE_VARIABLE_SIZE;

  return SBOX_ALL_OK;
}

// Construct the IPC server and the IPC dispatcher. When the target does
// an IPC it will eventually call the dispatcher.
ResultCode TargetProcess::Init(Dispatcher* ipc_dispatcher,
                               void* policy,
                               uint32_t shared_IPC_size,
                               uint32_t shared_policy_size,
                               DWORD* win_error) {
  // We need to map the shared memory on the target. This is necessary for
  // any IPC that needs to take place, even if the target has not yet hit
  // the main( ) function or even has initialized the CRT. So here we set
  // the handle to the shared section. The target on the first IPC must do
  // the rest, which boils down to calling MapViewofFile()

  // We use this single memory pool for IPC and for policy.
  DWORD shared_mem_size =
      static_cast<DWORD>(shared_IPC_size + shared_policy_size);
  shared_section_.Set(::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                           PAGE_READWRITE | SEC_COMMIT, 0,
                                           shared_mem_size, nullptr));
  if (!shared_section_.IsValid()) {
    *win_error = ::GetLastError();
    return SBOX_ERROR_CREATE_FILE_MAPPING;
  }

  void* shared_memory = ::MapViewOfFile(
      shared_section_.Get(), FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0);
  if (!shared_memory) {
    *win_error = ::GetLastError();
    return SBOX_ERROR_MAP_VIEW_OF_SHARED_SECTION;
  }

  CopyPolicyToTarget(policy, shared_policy_size,
                     reinterpret_cast<char*>(shared_memory) + shared_IPC_size);

  ResultCode ret;
  // Set the global variables in the target. These are not used on the broker.
  g_shared_IPC_size = shared_IPC_size;
  ret = TransferVariable("g_shared_IPC_size", &g_shared_IPC_size,
                         sizeof(g_shared_IPC_size));
  g_shared_IPC_size = 0;
  if (SBOX_ALL_OK != ret) {
    *win_error = ::GetLastError();
    return ret;
  }
  g_shared_policy_size = shared_policy_size;
  ret = TransferVariable("g_shared_policy_size", &g_shared_policy_size,
                         sizeof(g_shared_policy_size));
  g_shared_policy_size = 0;
  if (SBOX_ALL_OK != ret) {
    *win_error = ::GetLastError();
    return ret;
  }

  ipc_server_ = std::make_unique<SharedMemIPCServer>(
      sandbox_process_info_.process_handle(),
      sandbox_process_info_.process_id(), thread_pool_, ipc_dispatcher);

  if (!ipc_server_->Init(shared_memory, shared_IPC_size, kIPCChannelSize))
    return SBOX_ERROR_NO_SPACE;

  DWORD access = FILE_MAP_READ | FILE_MAP_WRITE | SECTION_QUERY;
  HANDLE target_shared_section;
  if (!::DuplicateHandle(::GetCurrentProcess(), shared_section_.Get(),
                         sandbox_process_info_.process_handle(),
                         &target_shared_section, access, false, 0)) {
    *win_error = ::GetLastError();
    return SBOX_ERROR_DUPLICATE_SHARED_SECTION;
  }

  g_shared_section = target_shared_section;
  ret = TransferVariable("g_shared_section", &g_shared_section,
                         sizeof(g_shared_section));
  g_shared_section = nullptr;
  if (SBOX_ALL_OK != ret) {
    *win_error = ::GetLastError();
    return ret;
  }

  // After this point we cannot use this handle anymore.
  ::CloseHandle(sandbox_process_info_.TakeThreadHandle());

  return SBOX_ALL_OK;
}

void TargetProcess::Terminate() {
  if (!sandbox_process_info_.IsValid())
    return;

  ::TerminateProcess(sandbox_process_info_.process_handle(), 0);
}

ResultCode TargetProcess::AssignLowBoxToken(
    const base::win::ScopedHandle& token) {
  if (!token.IsValid())
    return SBOX_ALL_OK;
  PROCESS_ACCESS_TOKEN process_access_token = {};
  process_access_token.token = token.Get();

  NtSetInformationProcess SetInformationProcess = nullptr;
  ResolveNTFunctionPtr("NtSetInformationProcess", &SetInformationProcess);

  NTSTATUS status = SetInformationProcess(
      sandbox_process_info_.process_handle(),
      static_cast<PROCESS_INFORMATION_CLASS>(NtProcessInformationAccessToken),
      &process_access_token, sizeof(process_access_token));
  if (!NT_SUCCESS(status)) {
    ::SetLastError(GetLastErrorFromNtStatus(status));
    return SBOX_ERROR_SET_LOW_BOX_TOKEN;
  }
  return SBOX_ALL_OK;
}

std::unique_ptr<TargetProcess> MakeTestTargetProcess(HANDLE process,
                                                     HMODULE base_address) {
  auto target = std::make_unique<TargetProcess>(
      base::win::ScopedHandle(), base::win::ScopedHandle(), nullptr, nullptr,
      std::vector<base::win::Sid>());
  PROCESS_INFORMATION process_info = {};
  process_info.hProcess = process;
  target->sandbox_process_info_.Set(process_info);
  target->base_address_ = base_address;
  return target;
}

}  // namespace sandbox
