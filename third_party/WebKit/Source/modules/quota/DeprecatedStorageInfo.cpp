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

#include "modules/quota/DeprecatedStorageInfo.h"

#include "core/dom/ExceptionCode.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/quota/DeprecatedStorageQuota.h"
#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageQuotaCallback.h"
#include "modules/quota/StorageUsageCallback.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

DeprecatedStorageInfo::DeprecatedStorageInfo() {}

void DeprecatedStorageInfo::queryUsageAndQuota(
    ScriptState* script_state,
    int storage_type,
    StorageUsageCallback* success_callback,
    StorageErrorCallback* error_callback) {
  // Dispatching the request to DeprecatedStorageQuota, as this interface is
  // deprecated in favor of DeprecatedStorageQuota.
  DeprecatedStorageQuota* storage_quota = GetStorageQuota(storage_type);
  if (!storage_quota) {
    // Unknown storage type is requested.
    TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, script_state)
        ->PostTask(BLINK_FROM_HERE, StorageErrorCallback::CreateSameThreadTask(
                                        error_callback, kNotSupportedError));
    return;
  }
  storage_quota->queryUsageAndQuota(script_state, success_callback,
                                    error_callback);
}

void DeprecatedStorageInfo::requestQuota(ScriptState* script_state,
                                         int storage_type,
                                         unsigned long long new_quota_in_bytes,
                                         StorageQuotaCallback* success_callback,
                                         StorageErrorCallback* error_callback) {
  // Dispatching the request to DeprecatedStorageQuota, as this interface is
  // deprecated in favor of DeprecatedStorageQuota.
  DeprecatedStorageQuota* storage_quota = GetStorageQuota(storage_type);
  if (!storage_quota) {
    // Unknown storage type is requested.
    TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, script_state)
        ->PostTask(BLINK_FROM_HERE, StorageErrorCallback::CreateSameThreadTask(
                                        error_callback, kNotSupportedError));
    return;
  }
  storage_quota->requestQuota(script_state, new_quota_in_bytes,
                              success_callback, error_callback);
}

DeprecatedStorageQuota* DeprecatedStorageInfo::GetStorageQuota(
    int storage_type) {
  switch (storage_type) {
    case kTemporary:
      if (!temporary_storage_)
        temporary_storage_ =
            DeprecatedStorageQuota::Create(DeprecatedStorageQuota::kTemporary);
      return temporary_storage_.Get();
    case kPersistent:
      if (!persistent_storage_)
        persistent_storage_ =
            DeprecatedStorageQuota::Create(DeprecatedStorageQuota::kPersistent);
      return persistent_storage_.Get();
  }
  return nullptr;
}

void DeprecatedStorageInfo::Trace(blink::Visitor* visitor) {
  visitor->Trace(temporary_storage_);
  visitor->Trace(persistent_storage_);
}

}  // namespace blink
