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

#include "third_party/blink/renderer/modules/quota/deprecated_storage_quota.h"

#include "base/location.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_error_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_estimate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_quota_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_usage_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/quota/dom_error.h"
#include "third_party/blink/renderer/modules/quota/quota_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::StorageType;
using mojom::blink::UsageBreakdownPtr;

namespace {

StorageType GetStorageType(DeprecatedStorageQuota::Type type) {
  switch (type) {
    case DeprecatedStorageQuota::kTemporary:
      return StorageType::kTemporary;
    case DeprecatedStorageQuota::kPersistent:
      return StorageType::kPersistent;
    default:
      return StorageType::kUnknown;
  }
}

void DeprecatedQueryStorageUsageAndQuotaCallback(
    V8StorageUsageCallback* success_callback,
    V8StorageErrorCallback* error_callback,
    mojom::blink::QuotaStatusCode status_code,
    int64_t usage_in_bytes,
    int64_t quota_in_bytes,
    UsageBreakdownPtr usage_breakdown) {
  if (status_code != mojom::blink::QuotaStatusCode::kOk) {
    if (error_callback) {
      error_callback->InvokeAndReportException(nullptr,
                                               DOMError::Create(status_code));
    }
    return;
  }

  if (success_callback) {
    success_callback->InvokeAndReportException(nullptr, usage_in_bytes,
                                               quota_in_bytes);
  }
}

void RequestStorageQuotaCallback(V8StorageQuotaCallback* success_callback,
                                 V8StorageErrorCallback* error_callback,
                                 mojom::blink::QuotaStatusCode status_code,
                                 int64_t usage_in_bytes,
                                 int64_t granted_quota_in_bytes) {
  if (status_code != mojom::blink::QuotaStatusCode::kOk) {
    if (error_callback) {
      error_callback->InvokeAndReportException(nullptr,
                                               DOMError::Create(status_code));
    }
    return;
  }

  if (success_callback) {
    success_callback->InvokeAndReportException(nullptr, granted_quota_in_bytes);
  }
}

}  // namespace

void DeprecatedStorageQuota::EnqueueStorageErrorCallback(
    ScriptState* script_state,
    V8StorageErrorCallback* error_callback,
    DOMExceptionCode exception_code) {
  if (!error_callback)
    return;

  ExecutionContext::From(script_state)
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&V8StorageErrorCallback::InvokeAndReportException,
                           WrapPersistent(error_callback), nullptr,
                           WrapPersistent(DOMError::Create(exception_code))));
}

DeprecatedStorageQuota::DeprecatedStorageQuota(Type type)
    : type_(type), quota_host_(nullptr) {}

void DeprecatedStorageQuota::queryUsageAndQuota(
    ScriptState* script_state,
    V8StorageUsageCallback* success_callback,
    V8StorageErrorCallback* error_callback) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  // The BlinkIDL definition for queryUsageAndQuota() already has a [Measure]
  // attribute, so the kQuotaRead use counter must be explicitly updated.
  UseCounter::Count(execution_context, WebFeature::kQuotaRead);

  StorageType storage_type = GetStorageType(type_);
  if (storage_type != StorageType::kTemporary &&
      storage_type != StorageType::kPersistent) {
    // Unknown storage type is requested.
    EnqueueStorageErrorCallback(script_state, error_callback,
                                DOMExceptionCode::kNotSupportedError);
    return;
  }

  const SecurityOrigin* security_origin =
      execution_context->GetSecurityOrigin();
  if (security_origin->IsOpaque()) {
    EnqueueStorageErrorCallback(script_state, error_callback,
                                DOMExceptionCode::kNotSupportedError);
    return;
  }

  auto callback = WTF::Bind(&DeprecatedQueryStorageUsageAndQuotaCallback,
                            WrapPersistent(success_callback),
                            WrapPersistent(error_callback));
  GetQuotaHost(execution_context)
      ->QueryStorageUsageAndQuota(
          storage_type,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback), mojom::blink::QuotaStatusCode::kErrorAbort,
              0, 0, nullptr));
}

void DeprecatedStorageQuota::requestQuota(
    ScriptState* script_state,
    uint64_t new_quota_in_bytes,
    V8StorageQuotaCallback* success_callback,
    V8StorageErrorCallback* error_callback) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // The BlinkIDL definition for requestQuota() already has a [Measure]
  // attribute, so the kQuotaRead use counter must be explicitly updated.
  UseCounter::Count(execution_context, WebFeature::kQuotaRead);

  StorageType storage_type = GetStorageType(type_);
  if (storage_type != StorageType::kTemporary &&
      storage_type != StorageType::kPersistent) {
    // Unknown storage type is requested.
    EnqueueStorageErrorCallback(script_state, error_callback,
                                DOMExceptionCode::kNotSupportedError);
    return;
  }

  auto callback =
      WTF::Bind(&RequestStorageQuotaCallback, WrapPersistent(success_callback),
                WrapPersistent(error_callback));

  if (execution_context->GetSecurityOrigin()->IsOpaque()) {
    // Unique origins cannot store persistent state.
    std::move(callback).Run(mojom::blink::QuotaStatusCode::kErrorAbort, 0, 0);
    return;
  }

  GetQuotaHost(execution_context)
      ->RequestStorageQuota(
          storage_type, new_quota_in_bytes,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback), mojom::blink::QuotaStatusCode::kErrorAbort,
              0, 0));
}

void DeprecatedStorageQuota::Trace(Visitor* visitor) {
  visitor->Trace(quota_host_);
  ScriptWrappable::Trace(visitor);
}

mojom::blink::QuotaManagerHost* DeprecatedStorageQuota::GetQuotaHost(
    ExecutionContext* execution_context) {
  if (!quota_host_.is_bound()) {
    ConnectToQuotaManagerHost(
        execution_context,
        quota_host_.BindNewPipeAndPassReceiver(execution_context->GetTaskRunner(
            blink::TaskType::kInternalDefault)));
  }
  return quota_host_.get();
}

}  // namespace blink
