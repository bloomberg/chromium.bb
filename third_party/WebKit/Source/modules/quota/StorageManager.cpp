// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/quota/StorageManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/V8StorageEstimate.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/UserGestureIndicator.h"
#include "modules/permissions/PermissionUtils.h"
#include "modules/quota/StorageEstimate.h"
#include "platform/StorageQuotaCallbacks.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Functional.h"
#include "public/platform/Platform.h"
#include "public/platform/WebStorageQuotaError.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

const char kUniqueOriginErrorMessage[] =
    "The operation is not supported in this context.";

class EstimateCallbacks final : public StorageQuotaCallbacks {
  WTF_MAKE_NONCOPYABLE(EstimateCallbacks);

 public:
  explicit EstimateCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  ~EstimateCallbacks() override {}

  void DidQueryStorageUsageAndQuota(
      unsigned long long usage_in_bytes,
      unsigned long long quota_in_bytes) override {
    StorageEstimate estimate;
    estimate.setUsage(usage_in_bytes);
    estimate.setQuota(quota_in_bytes);
    resolver_->Resolve(estimate);
  }

  void DidFail(WebStorageQuotaError error) override {
    resolver_->Reject(DOMException::Create(static_cast<ExceptionCode>(error)));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(resolver_);
    StorageQuotaCallbacks::Trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace

ScriptPromise StorageManager::persist(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsSecureContext());  // [SecureContext] in IDL
  SecurityOrigin* security_origin = execution_context->GetSecurityOrigin();
  if (security_origin->IsUnique()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), kUniqueOriginErrorMessage));
    return promise;
  }

  DCHECK(execution_context->IsDocument());
  PermissionService* permission_service =
      GetPermissionService(ExecutionContext::From(script_state));
  if (!permission_service) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "In its current state, the global scope can't request permissions."));
    return promise;
  }
  permission_service->RequestPermission(
      CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE),
      ExecutionContext::From(script_state)->GetSecurityOrigin(),
      UserGestureIndicator::ProcessingUserGesture(),
      ConvertToBaseCallback(
          WTF::Bind(&StorageManager::PermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver))));

  return promise;
}

ScriptPromise StorageManager::persisted(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsSecureContext());  // [SecureContext] in IDL
  SecurityOrigin* security_origin = execution_context->GetSecurityOrigin();
  if (security_origin->IsUnique()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), kUniqueOriginErrorMessage));
    return promise;
  }

  PermissionService* permission_service =
      GetPermissionService(ExecutionContext::From(script_state));
  if (!permission_service) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "In its current state, the global scope can't query permissions."));
    return promise;
  }
  permission_service->HasPermission(
      CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE),
      ExecutionContext::From(script_state)->GetSecurityOrigin(),
      ConvertToBaseCallback(
          WTF::Bind(&StorageManager::PermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver))));
  return promise;
}

ScriptPromise StorageManager::estimate(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsSecureContext());  // [SecureContext] in IDL
  SecurityOrigin* security_origin = execution_context->GetSecurityOrigin();
  if (security_origin->IsUnique()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), kUniqueOriginErrorMessage));
    return promise;
  }

  KURL storage_partition = KURL(NullURL(), security_origin->ToString());
  Platform::Current()->QueryStorageUsageAndQuota(
      storage_partition, kWebStorageQuotaTypeTemporary,
      new EstimateCallbacks(resolver));
  return promise;
}

DEFINE_TRACE(StorageManager) {}

PermissionService* StorageManager::GetPermissionService(
    ExecutionContext* execution_context) {
  if (!permission_service_ &&
      ConnectToPermissionService(execution_context,
                                 mojo::MakeRequest(&permission_service_)))
    permission_service_.set_connection_error_handler(ConvertToBaseCallback(
        WTF::Bind(&StorageManager::PermissionServiceConnectionError,
                  WrapWeakPersistent(this))));
  return permission_service_.get();
}

void StorageManager::PermissionServiceConnectionError() {
  permission_service_.reset();
}

void StorageManager::PermissionRequestComplete(ScriptPromiseResolver* resolver,
                                               PermissionStatus status) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver->Resolve(status == PermissionStatus::GRANTED);
}

STATIC_ASSERT_ENUM(kWebStorageQuotaErrorNotSupported, kNotSupportedError);
STATIC_ASSERT_ENUM(kWebStorageQuotaErrorInvalidModification,
                   kInvalidModificationError);
STATIC_ASSERT_ENUM(kWebStorageQuotaErrorInvalidAccess, kInvalidAccessError);
STATIC_ASSERT_ENUM(kWebStorageQuotaErrorAbort, kAbortError);

}  // namespace blink
