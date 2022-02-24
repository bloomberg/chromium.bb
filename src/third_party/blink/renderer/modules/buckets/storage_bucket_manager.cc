// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/buckets/storage_bucket_manager.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_bucket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

namespace blink {

namespace {

bool IsValidName(const String& name) {
  if (!name.IsLowerASCII())
    return false;

  if (!name.ContainsOnlyASCIIOrEmpty())
    return false;

  if (name.IsEmpty() || name.length() >= 64)
    return false;

  // | name | must only contain lowercase latin letters, digits 0-9, or special
  // characters '-' & '_' in the middle of the name, but not at the beginning.
  for (wtf_size_t i = 0; i < name.length(); i++) {
    if (!IsASCIIAlphanumeric(name[i]) &&
        (i == 0 || (name[i] != '_' && name[i] != '-'))) {
      return false;
    }
  }
  return true;
}

mojom::blink::BucketPoliciesPtr ToMojoBucketPolicies(
    const StorageBucketOptions* options) {
  auto policies = mojom::blink::BucketPolicies::New();
  policies->persisted = options->persisted();
  policies->quota = options->hasQuotaNonNull()
                        ? options->quotaNonNull()
                        : mojom::blink::kNoQuotaPolicyValue;

  if (options->durability() == "strict") {
    policies->durability = mojom::blink::BucketDurability::kStrict;
  } else {
    policies->durability = mojom::blink::BucketDurability::kRelaxed;
  }

  if (options->hasExpiresNonNull())
    policies->expires = base::Time::FromJavaTime(options->expiresNonNull());
  return policies;
}

}  // namespace

const char StorageBucketManager::kSupplementName[] = "StorageBucketManager";

StorageBucketManager::StorageBucketManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextClient(navigator.GetExecutionContext()),
      manager_remote_(navigator.GetExecutionContext()) {}

StorageBucketManager* StorageBucketManager::storageBuckets(
    ScriptState* script_state,
    NavigatorBase& navigator,
    ExceptionState& exception_state) {
  auto* supplement =
      Supplement<NavigatorBase>::From<StorageBucketManager>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<StorageBucketManager>(navigator);
    Supplement<NavigatorBase>::ProvideTo(navigator, supplement);
  }
  return supplement;
}

ScriptPromise StorageBucketManager::open(ScriptState* script_state,
                                         const String& name,
                                         const StorageBucketOptions* options,
                                         ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessStorageBuckets()) {
    exception_state.ThrowSecurityError(
        "Access to Storage Buckets API is denied in this context.");
    return promise;
  }

  if (!IsValidName(name)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidCharacterError,
        "The bucket name '" + name + "' is not a valid name."));
    return promise;
  }

  mojom::blink::BucketPoliciesPtr bucket_policies =
      ToMojoBucketPolicies(options);
  GetBucketManager(script_state)
      ->OpenBucket(name, std::move(bucket_policies),
                   WTF::Bind(&StorageBucketManager::DidOpen,
                             WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageBucketManager::keys(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessStorageBuckets()) {
    exception_state.ThrowSecurityError(
        "Access to Storage Buckets API is denied in this context.");
    return promise;
  }

  GetBucketManager(script_state)
      ->Keys(WTF::Bind(&StorageBucketManager::DidGetKeys, WrapPersistent(this),
                       WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageBucketManager::Delete(ScriptState* script_state,
                                           const String& name,
                                           ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessStorageBuckets()) {
    exception_state.ThrowSecurityError(
        "Access to Storage Buckets API is denied in this context.");
    return promise;
  }

  if (!IsValidName(name)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidCharacterError,
        "The bucket name " + name + " is not a valid name."));
    return promise;
  }

  GetBucketManager(script_state)
      ->DeleteBucket(name,
                     WTF::Bind(&StorageBucketManager::DidDelete,
                               WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

mojom::blink::BucketManagerHost* StorageBucketManager::GetBucketManager(
    ScriptState* script_state) {
  if (!manager_remote_.is_bound()) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    mojo::PendingReceiver<mojom::blink::BucketManagerHost> receiver =
        manager_remote_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI));
    context->GetBrowserInterfaceBroker().GetInterface(std::move(receiver));
  }
  DCHECK(manager_remote_.is_bound());
  return manager_remote_.get();
}

void StorageBucketManager::DidOpen(
    ScriptPromiseResolver* resolver,
    mojo::PendingRemote<mojom::blink::BucketHost> bucket_remote) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!bucket_remote) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while creating a bucket."));
    return;
  }
  resolver->Resolve(MakeGarbageCollected<StorageBucket>(
      GetExecutionContext(), std::move(bucket_remote)));
}

void StorageBucketManager::DidGetKeys(ScriptPromiseResolver* resolver,
                                      const Vector<String>& keys,
                                      bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while retrieving bucket names."));
    return;
  }
  resolver->Resolve(keys);
}

void StorageBucketManager::DidDelete(ScriptPromiseResolver* resolver,
                                     bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occured while deleting a bucket."));
    return;
  }
  resolver->Resolve();
}

void StorageBucketManager::Trace(Visitor* visitor) const {
  visitor->Trace(manager_remote_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
