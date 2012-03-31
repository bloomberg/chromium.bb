// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/handle_interception.h"

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "sandbox/src/target_services.h"

namespace sandbox {

ResultCode DuplicateHandleProxy(HANDLE source_handle,
                                DWORD target_process_id,
                                HANDLE* target_handle,
                                DWORD desired_access,
                                DWORD options) {
  *target_handle = NULL;

  void* memory = GetGlobalIPCMemory();
  if (NULL == memory)
    return SBOX_ERROR_NO_SPACE;

  SharedMemIPCClient ipc(memory);
  CrossCallReturn answer = {0};
  ResultCode code = CrossCall(ipc, IPC_DUPLICATEHANDLEPROXY_TAG,
                              source_handle, target_process_id,
                              desired_access, options, &answer);
  if (SBOX_ALL_OK != code)
    return code;

  if (answer.win32_result) {
    ::SetLastError(answer.nt_status);
    return SBOX_ERROR_GENERIC;
  }

  *target_handle = answer.handle;
  return SBOX_ALL_OK;
}

}  // namespace sandbox

