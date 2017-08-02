/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/quota/DeprecatedStorageQuota.h"

#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/quota/DeprecatedStorageQuotaCallbacksImpl.h"
#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageQuotaClient.h"
#include "modules/quota/StorageUsageCallback.h"
#include "platform/StorageQuotaCallbacks.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/ScriptState.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebStorageQuotaCallbacks.h"
#include "public/platform/WebStorageQuotaType.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

DeprecatedStorageQuota::DeprecatedStorageQuota(Type type) : type_(type) {}

void DeprecatedStorageQuota::queryUsageAndQuota(
    ScriptState* script_state,
    StorageUsageCallback* success_callback,
    StorageErrorCallback* error_callback) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  WebStorageQuotaType storage_type = static_cast<WebStorageQuotaType>(type_);
  if (storage_type != kWebStorageQuotaTypeTemporary &&
      storage_type != kWebStorageQuotaTypePersistent) {
    // Unknown storage type is requested.
    TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, script_state)
        ->PostTask(BLINK_FROM_HERE, StorageErrorCallback::CreateSameThreadTask(
                                        error_callback, kNotSupportedError));
    return;
  }

  SecurityOrigin* security_origin = execution_context->GetSecurityOrigin();
  if (security_origin->IsUnique()) {
    TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, script_state)
        ->PostTask(BLINK_FROM_HERE, StorageErrorCallback::CreateSameThreadTask(
                                        error_callback, kNotSupportedError));
    return;
  }

  KURL storage_partition = KURL(NullURL(), security_origin->ToString());
  StorageQuotaCallbacks* callbacks =
      DeprecatedStorageQuotaCallbacksImpl::Create(success_callback,
                                                  error_callback);
  Platform::Current()->QueryStorageUsageAndQuota(storage_partition,
                                                 storage_type, callbacks);
}

void DeprecatedStorageQuota::requestQuota(
    ScriptState* script_state,
    unsigned long long new_quota_in_bytes,
    StorageQuotaCallback* success_callback,
    StorageErrorCallback* error_callback) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  WebStorageQuotaType storage_type = static_cast<WebStorageQuotaType>(type_);
  if (storage_type != kWebStorageQuotaTypeTemporary &&
      storage_type != kWebStorageQuotaTypePersistent) {
    // Unknown storage type is requested.
    TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, script_state)
        ->PostTask(BLINK_FROM_HERE, StorageErrorCallback::CreateSameThreadTask(
                                        error_callback, kNotSupportedError));
    return;
  }

  StorageQuotaClient* client = StorageQuotaClient::From(execution_context);
  if (!client) {
    TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, script_state)
        ->PostTask(BLINK_FROM_HERE, StorageErrorCallback::CreateSameThreadTask(
                                        error_callback, kNotSupportedError));
    return;
  }

  client->RequestQuota(script_state, storage_type, new_quota_in_bytes,
                       success_callback, error_callback);
}

STATIC_ASSERT_ENUM(kWebStorageQuotaTypeTemporary,
                   DeprecatedStorageQuota::kTemporary);
STATIC_ASSERT_ENUM(kWebStorageQuotaTypePersistent,
                   DeprecatedStorageQuota::kPersistent);

}  // namespace blink
