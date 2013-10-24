// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "sandbox/win/src/sync_policy.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/sync_interception.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

NTSTATUS GetBaseNamedObjectsDirectory(HANDLE* directory) {
  static HANDLE base_named_objects_handle = NULL;
  if (base_named_objects_handle) {
    *directory = base_named_objects_handle;
    return STATUS_SUCCESS;
  }

  NtOpenDirectoryObjectFunction NtOpenDirectoryObject = NULL;
  ResolveNTFunctionPtr("NtOpenDirectoryObject", &NtOpenDirectoryObject);

  DWORD session_id = 0;
  ProcessIdToSessionId(::GetCurrentProcessId(), &session_id);

  std::wstring base_named_objects_path =
      base::StringPrintf(L"\\Sessions\\%d\\BaseNamedObjects", session_id);

  UNICODE_STRING directory_name = {};
  OBJECT_ATTRIBUTES object_attributes = {};
  InitObjectAttribs(base_named_objects_path, OBJ_CASE_INSENSITIVE, NULL,
                    &object_attributes, &directory_name);
  NTSTATUS status = NtOpenDirectoryObject(&base_named_objects_handle,
                                          DIRECTORY_ALL_ACCESS,
                                          &object_attributes);
  if (status == STATUS_SUCCESS)
    *directory = base_named_objects_handle;
  return status;
}

bool SyncPolicy::GenerateRules(const wchar_t* name,
                               TargetPolicy::Semantics semantics,
                               LowLevelPolicy* policy) {
  std::wstring mod_name(name);
  if (mod_name.empty()) {
    return false;
  }

  if (TargetPolicy::EVENTS_ALLOW_ANY != semantics &&
      TargetPolicy::EVENTS_ALLOW_READONLY != semantics) {
    // Other flags are not valid for sync policy yet.
    NOTREACHED();
    return false;
  }

  // Add the open rule.
  EvalResult result = ASK_BROKER;
  PolicyRule open(result);

  if (!open.AddStringMatch(IF, OpenEventParams::NAME, name, CASE_INSENSITIVE))
    return false;

  if (TargetPolicy::EVENTS_ALLOW_READONLY == semantics) {
    // We consider all flags that are not known to be readonly as potentially
    // used for write.
    DWORD allowed_flags = SYNCHRONIZE | GENERIC_READ | READ_CONTROL;
    DWORD restricted_flags = ~allowed_flags;
    open.AddNumberMatch(IF_NOT, OpenEventParams::ACCESS, restricted_flags, AND);
  }

  if (!policy->AddRule(IPC_OPENEVENT_TAG, &open))
    return false;

  // If it's not a read only, add the create rule.
  if (TargetPolicy::EVENTS_ALLOW_READONLY != semantics) {
    PolicyRule create(result);
    if (!create.AddStringMatch(IF, NameBased::NAME, name, CASE_INSENSITIVE))
      return false;

    if (!policy->AddRule(IPC_CREATEEVENT_TAG, &create))
      return false;
  }

  return true;
}

DWORD SyncPolicy::CreateEventAction(EvalResult eval_result,
                                    const ClientInfo& client_info,
                                    const std::wstring &event_name,
                                    uint32 event_type,
                                    uint32 initial_state,
                                    HANDLE *handle) {
  NtCreateEventFunction NtCreateEvent = NULL;
  ResolveNTFunctionPtr("NtCreateEvent", &NtCreateEvent);

  // The only action supported is ASK_BROKER which means create the requested
  // file as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE object_directory = NULL;
  NTSTATUS status = GetBaseNamedObjectsDirectory(&object_directory);
  if (status != STATUS_SUCCESS)
    return status;

  UNICODE_STRING unicode_event_name = {};
  OBJECT_ATTRIBUTES object_attributes = {};
  InitObjectAttribs(event_name, OBJ_CASE_INSENSITIVE, object_directory,
                    &object_attributes, &unicode_event_name);

  HANDLE local_handle = NULL;
  status = NtCreateEvent(&local_handle, EVENT_ALL_ACCESS, &object_attributes,
                         static_cast<EVENT_TYPE>(event_type), initial_state);
  if (NULL == local_handle)
    return status;

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, FALSE,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return STATUS_ACCESS_DENIED;
  }
  return status;
}

DWORD SyncPolicy::OpenEventAction(EvalResult eval_result,
                                  const ClientInfo& client_info,
                                  const std::wstring &event_name,
                                  uint32 desired_access,
                                  HANDLE *handle) {
  NtOpenEventFunction NtOpenEvent = NULL;
  ResolveNTFunctionPtr("NtOpenEvent", &NtOpenEvent);

  // The only action supported is ASK_BROKER which means create the requested
  // event as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE object_directory = NULL;
  NTSTATUS status = GetBaseNamedObjectsDirectory(&object_directory);
  if (status != STATUS_SUCCESS)
    return status;

  UNICODE_STRING unicode_event_name = {};
  OBJECT_ATTRIBUTES object_attributes = {};
  InitObjectAttribs(event_name, OBJ_CASE_INSENSITIVE, object_directory,
                    &object_attributes, &unicode_event_name);

  HANDLE local_handle = NULL;
  status = NtOpenEvent(&local_handle, desired_access, &object_attributes);
  if (NULL == local_handle)
    return status;

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, FALSE,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return STATUS_ACCESS_DENIED;
  }
  return status;
}

}  // namespace sandbox
