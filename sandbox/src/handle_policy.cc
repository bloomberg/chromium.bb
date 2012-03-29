// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/handle_policy.h"

#include <string>

#include "base/win/scoped_handle.h"
#include "sandbox/src/broker_services.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_engine_opcodes.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/sandbox_types.h"
#include "sandbox/src/sandbox_utils.h"

namespace sandbox {

bool HandlePolicy::GenerateRules(const wchar_t* type_name,
                                 TargetPolicy::Semantics semantics,
                                 LowLevelPolicy* policy) {
  // We don't support any other semantics for handles yet.
  if (TargetPolicy::HANDLES_DUP_ANY != semantics) {
    return false;
  }
  PolicyRule duplicate_rule(ASK_BROKER);
  if (!duplicate_rule.AddStringMatch(IF, NameBased::NAME, type_name,
                                     CASE_INSENSITIVE)) {
    return false;
  }
  if (!policy->AddRule(IPC_DUPLICATEHANDLEPROXY_TAG, &duplicate_rule)) {
    return false;
  }
  return true;
}

DWORD HandlePolicy::DuplicateHandleProxyAction(EvalResult eval_result,
                                               const ClientInfo& client_info,
                                               HANDLE source_handle,
                                               DWORD target_process_id,
                                               HANDLE* target_handle,
                                               DWORD desired_access,
                                               DWORD options) {
  // The only action supported is ASK_BROKER which means duplicate the handle.
  if (ASK_BROKER != eval_result) {
    return ERROR_ACCESS_DENIED;
  }

  // Make sure the target is one of our sandboxed children.
  if (!BrokerServicesBase::GetInstance()->IsActiveTarget(target_process_id)) {
    return ERROR_ACCESS_DENIED;
  }

  base::win::ScopedHandle target_process(::OpenProcess(PROCESS_DUP_HANDLE,
                                                       FALSE,
                                                       target_process_id));
  if (NULL == target_process)
    return ::GetLastError();

  DWORD result = ERROR_SUCCESS;
  if (!::DuplicateHandle(client_info.process, source_handle, target_process,
                         target_handle, desired_access, FALSE,
                         options)) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

}  // namespace sandbox

