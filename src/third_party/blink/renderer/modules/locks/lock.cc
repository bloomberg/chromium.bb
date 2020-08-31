// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/locks/lock.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/locks/lock_manager.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
const char* kLockModeNameExclusive = "exclusive";
const char* kLockModeNameShared = "shared";
}  // namespace

class Lock::ThenFunction final : public ScriptFunction {
 public:
  enum ResolveType {
    Fulfilled,
    Rejected,
  };

  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                Lock* lock,
                                                ResolveType type) {
    ThenFunction* self =
        MakeGarbageCollected<ThenFunction>(script_state, lock, type);
    return self->BindToV8Function();
  }

  ThenFunction(ScriptState* script_state, Lock* lock, ResolveType type)
      : ScriptFunction(script_state), lock_(lock), resolve_type_(type) {}

  void Trace(Visitor* visitor) override {
    visitor->Trace(lock_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    DCHECK(lock_);
    DCHECK(resolve_type_ == Fulfilled || resolve_type_ == Rejected);
    lock_->ReleaseIfHeld();
    if (resolve_type_ == Fulfilled)
      lock_->resolver_->Resolve(value);
    else
      lock_->resolver_->Reject(value);
    lock_ = nullptr;
    return value;
  }

  Member<Lock> lock_;
  ResolveType resolve_type_;
};

Lock::Lock(ScriptState* script_state,
           const String& name,
           mojom::blink::LockMode mode,
           mojo::PendingAssociatedRemote<mojom::blink::LockHandle> handle,
           mojo::PendingRemote<mojom::blink::ObservedFeature> lock_lifetime,
           LockManager* manager)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      name_(name),
      mode_(mode),
      handle_(ExecutionContext::From(script_state)),
      lock_lifetime_(ExecutionContext::From(script_state)),
      manager_(manager) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)->GetTaskRunner(TaskType::kWebLocks);
  handle_.Bind(std::move(handle), task_runner);
  lock_lifetime_.Bind(std::move(lock_lifetime), task_runner);
  handle_.set_disconnect_handler(
      WTF::Bind(&Lock::OnConnectionError, WrapWeakPersistent(this)));
}

Lock::~Lock() = default;

String Lock::mode() const {
  return ModeToString(mode_);
}

void Lock::HoldUntil(ScriptPromise promise, ScriptPromiseResolver* resolver) {
  DCHECK(handle_.is_bound());
  DCHECK(!resolver_);

  ScriptState* script_state = resolver->GetScriptState();
  resolver_ = resolver;
  promise.Then(
      ThenFunction::CreateFunction(script_state, this, ThenFunction::Fulfilled),
      ThenFunction::CreateFunction(script_state, this, ThenFunction::Rejected));
}

// static
mojom::blink::LockMode Lock::StringToMode(const String& string) {
  if (string == kLockModeNameShared)
    return mojom::blink::LockMode::SHARED;
  if (string == kLockModeNameExclusive)
    return mojom::blink::LockMode::EXCLUSIVE;
  NOTREACHED();
  return mojom::blink::LockMode::SHARED;
}

// static
String Lock::ModeToString(mojom::blink::LockMode mode) {
  switch (mode) {
    case mojom::blink::LockMode::SHARED:
      return kLockModeNameShared;
    case mojom::blink::LockMode::EXCLUSIVE:
      return kLockModeNameExclusive;
  }
  NOTREACHED();
  return g_empty_string;
}

void Lock::ContextDestroyed() {
  ReleaseIfHeld();
}

void Lock::Trace(Visitor* visitor) {
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
  visitor->Trace(resolver_);
  visitor->Trace(handle_);
  visitor->Trace(lock_lifetime_);
  visitor->Trace(manager_);
}

void Lock::ReleaseIfHeld() {
  if (handle_.is_bound()) {
    // Drop the mojo pipe; this releases the lock on the back end.
    handle_.reset();

    lock_lifetime_.reset();

    // Let the lock manager know that this instance can be collected.
    manager_->OnLockReleased(this);
  }
}

void Lock::OnConnectionError() {
  ReleaseIfHeld();
  resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError,
      "Lock broken by another request with the 'steal' option."));
}

}  // namespace blink
