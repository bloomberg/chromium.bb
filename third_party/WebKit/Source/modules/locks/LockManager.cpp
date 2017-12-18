// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/locks/LockManager.h"

#include <algorithm>
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/v8_lock_granted_callback.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/UseCounter.h"
#include "modules/locks/Lock.h"
#include "modules/locks/LockOptions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/bindings/Microtask.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

class LockManager::LockRequestImpl final
    : public GarbageCollectedFinalized<LockRequestImpl>,
      public TraceWrapperBase,
      public mojom::blink::LockRequest {
  WTF_MAKE_NONCOPYABLE(LockRequestImpl);
  EAGERLY_FINALIZE();

 public:
  LockRequestImpl(V8LockGrantedCallback* callback,
                  ScriptPromiseResolver* resolver,
                  const String& name,
                  mojom::blink::LockManager::LockMode mode,
                  mojom::blink::LockRequestRequest request,
                  LockManager* manager)
      : callback_(callback),
        resolver_(resolver),
        name_(name),
        mode_(mode),
        binding_(this, std::move(request)),
        manager_(manager) {}

  ~LockRequestImpl() = default;

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(resolver_);
    visitor->Trace(manager_);
    visitor->Trace(callback_);
  }

  // Wrapper tracing is needed for callbacks. The reference chain is
  // NavigatorLocksImpl -> LockManager -> LockRequestImpl ->
  // V8LockGrantedCallback.
  void TraceWrappers(const ScriptWrappableVisitor* visitor) const override {
    visitor->TraceWrappers(callback_);
  }

  // Called to immediately close the pipe which signals the back-end,
  // unblocking further requests, without waiting for GC finalize the object.
  void Cancel() { binding_.Close(); }

  void Abort(const String& reason) override {
    manager_->RemovePendingRequest(this);
    binding_.Close();

    if (!resolver_->GetScriptState()->ContextIsValid())
      return;

    resolver_->Reject(DOMException::Create(kAbortError, reason));
  }

  void Failed() override {
    manager_->RemovePendingRequest(this);
    binding_.Close();

    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid())
      return;

    // Lock was not granted e.g. because ifAvailable was specified but
    // the lock was not available.
    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    v8::Maybe<ScriptValue> result = callback_->Invoke(nullptr, nullptr);
    if (try_catch.HasCaught()) {
      resolver_->Reject(try_catch.Exception());
    } else if (!result.IsNothing()) {
      resolver_->Resolve(result.FromJust());
    }
  }

  void Granted(mojom::blink::LockHandlePtr handle) override {
    DCHECK(binding_.is_bound());
    DCHECK(handle.is_bound());

    manager_->RemovePendingRequest(this);
    binding_.Close();

    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      // If a handle was returned, it will be automatically be released.
      return;
    }

    Lock* lock = Lock::Create(script_state, name_, mode_, std::move(handle));

    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    v8::Maybe<ScriptValue> result = callback_->Invoke(nullptr, lock);
    if (try_catch.HasCaught()) {
      lock->HoldUntil(
          ScriptPromise::Reject(script_state, try_catch.Exception()),
          resolver_);
    } else if (!result.IsNothing()) {
      lock->HoldUntil(ScriptPromise::Cast(script_state, result.FromJust()),
                      resolver_);
    }
  }

 private:
  // Callback passed by script; invoked when the lock is granted.
  TraceWrapperMember<V8LockGrantedCallback> callback_;

  // Rejects if the request was aborted, otherwise resolves/rejects with
  // |callback_|'s result.
  Member<ScriptPromiseResolver> resolver_;

  // Held to stamp the Lock object's |name| property.
  String name_;

  // Held to stamp the Lock object's |mode| property.
  mojom::blink::LockManager::LockMode mode_;

  mojo::Binding<mojom::blink::LockRequest> binding_;

  // The |manager_| keeps |this| alive until a response comes in and this is
  // registered. If the context is destroyed then |manager_| will dispose of
  // |this| which terminates the request on the service side.
  Member<LockManager> manager_;
};

LockManager::LockManager(ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

ScriptPromise LockManager::acquire(ScriptState* script_state,
                                   const String& name,
                                   V8LockGrantedCallback* callback,
                                   ExceptionState& exception_state) {
  return acquire(script_state, name, LockOptions(), callback, exception_state);
}
ScriptPromise LockManager::acquire(ScriptState* script_state,
                                   const String& name,
                                   const LockOptions& options,
                                   V8LockGrantedCallback* callback,
                                   ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!context->GetSecurityOrigin()->CanAccessLocks()) {
    exception_state.ThrowSecurityError(
        "Access to the Locks API is denied in this context.");
    return ScriptPromise();
  } else if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedLocks);
  }

  if (!service_.get()) {
    if (auto* provider = context->GetInterfaceProvider())
      provider->GetInterface(mojo::MakeRequest(&service_));
    if (!service_.get()) {
      exception_state.ThrowTypeError("Service not available.");
      return ScriptPromise();
    }
  }

  mojom::blink::LockManager::LockMode mode = Lock::StringToMode(options.mode());

  mojom::blink::LockManager::WaitMode wait =
      options.ifAvailable() ? mojom::blink::LockManager::WaitMode::NO_WAIT
                            : mojom::blink::LockManager::WaitMode::WAIT;

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  mojom::blink::LockRequestPtr request_ptr;
  AddPendingRequest(new LockRequestImpl(callback, resolver, name, mode,
                                        mojo::MakeRequest(&request_ptr), this));

  service_->RequestLock(
      ExecutionContext::From(script_state)->GetSecurityOrigin(), name, mode,
      wait, std::move(request_ptr));

  return promise;
}

void LockManager::AddPendingRequest(LockRequestImpl* request) {
  pending_requests_.insert(request);
}

void LockManager::RemovePendingRequest(LockRequestImpl* request) {
  pending_requests_.erase(request);
}

void LockManager::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(pending_requests_);
}

void LockManager::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  for (auto request : pending_requests_)
    visitor->TraceWrappers(request);
}

void LockManager::ContextDestroyed(ExecutionContext*) {
  for (auto request : pending_requests_)
    request->Cancel();
  pending_requests_.clear();
}

}  // namespace blink
