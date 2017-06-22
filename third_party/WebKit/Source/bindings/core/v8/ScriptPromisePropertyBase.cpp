// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptPromisePropertyBase.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

ScriptPromisePropertyBase::ScriptPromisePropertyBase(
    ExecutionContext* execution_context,
    Name name)
    : ContextClient(execution_context),
      isolate_(ToIsolate(execution_context)),
      name_(name),
      state_(kPending) {}

ScriptPromisePropertyBase::~ScriptPromisePropertyBase() {
  // TODO(haraken): Stop calling ClearWrappers here, as the dtor is invoked
  // during oilpan GC, but ClearWrappers potentially runs user script.
  ClearWrappers();
}

ScriptPromise ScriptPromisePropertyBase::Promise(DOMWrapperWorld& world) {
  if (!GetExecutionContext())
    return ScriptPromise();

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = ToV8Context(GetExecutionContext(), world);
  if (context.IsEmpty())
    return ScriptPromise();
  ScriptState* script_state = ScriptState::From(context);
  ScriptState::Scope scope(script_state);

  v8::Local<v8::Object> wrapper = EnsureHolderWrapper(script_state);
  DCHECK(wrapper->CreationContext() == context);

  v8::Local<v8::Value> cached_promise = PromiseSymbol().GetOrUndefined(wrapper);
  if (!cached_promise->IsUndefined() && cached_promise->IsPromise())
    return ScriptPromise(script_state, cached_promise);

  // Create and cache the Promise
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver))
    return ScriptPromise();
  v8::Local<v8::Promise> promise = resolver->GetPromise();
  PromiseSymbol().Set(wrapper, promise);

  switch (state_) {
    case kPending:
      // Cache the resolver too
      ResolverSymbol().Set(wrapper, resolver);
      break;
    case kResolved:
    case kRejected:
      ResolveOrRejectInternal(resolver);
      break;
  }

  return ScriptPromise(script_state, promise);
}

void ScriptPromisePropertyBase::ResolveOrReject(State target_state) {
  DCHECK(GetExecutionContext());
  DCHECK_EQ(state_, kPending);
  DCHECK(target_state == kResolved || target_state == kRejected);

  state_ = target_state;

  v8::HandleScope handle_scope(isolate_);
  size_t i = 0;
  while (i < wrappers_.size()) {
    const std::unique_ptr<ScopedPersistent<v8::Object>>& persistent =
        wrappers_[i];
    if (persistent->IsEmpty()) {
      // wrapper has died.
      // Since v8 GC can run during the iteration and clear the reference,
      // we can't move this check out of the loop.
      wrappers_.erase(i);
      continue;
    }
    v8::Local<v8::Object> wrapper = persistent->NewLocal(isolate_);
    ScriptState* script_state = ScriptState::From(wrapper->CreationContext());
    ScriptState::Scope scope(script_state);

    V8PrivateProperty::Symbol symbol = ResolverSymbol();
    DCHECK(symbol.HasValue(wrapper));
    v8::Local<v8::Promise::Resolver> resolver =
        symbol.GetOrUndefined(wrapper).As<v8::Promise::Resolver>();

    symbol.DeleteProperty(wrapper);
    ResolveOrRejectInternal(resolver);
    ++i;
  }
}

void ScriptPromisePropertyBase::ResetBase() {
  CheckThis();
  ClearWrappers();
  state_ = kPending;
}

void ScriptPromisePropertyBase::ResolveOrRejectInternal(
    v8::Local<v8::Promise::Resolver> resolver) {
  v8::Local<v8::Context> context = resolver->CreationContext();
  switch (state_) {
    case kPending:
      NOTREACHED();
      break;
    case kResolved:
      resolver->Resolve(context, ResolvedValue(isolate_, context->Global()))
          .ToChecked();
      break;
    case kRejected:
      resolver->Reject(context, RejectedValue(isolate_, context->Global()))
          .ToChecked();
      break;
  }
}

v8::Local<v8::Object> ScriptPromisePropertyBase::EnsureHolderWrapper(
    ScriptState* script_state) {
  v8::Local<v8::Context> context = script_state->GetContext();
  size_t i = 0;
  while (i < wrappers_.size()) {
    const std::unique_ptr<ScopedPersistent<v8::Object>>& persistent =
        wrappers_[i];
    if (persistent->IsEmpty()) {
      // wrapper has died.
      // Since v8 GC can run during the iteration and clear the reference,
      // we can't move this check out of the loop.
      wrappers_.erase(i);
      continue;
    }

    v8::Local<v8::Object> wrapper = persistent->NewLocal(isolate_);
    if (wrapper->CreationContext() == context)
      return wrapper;
    ++i;
  }
  v8::Local<v8::Object> wrapper = Holder(isolate_, context->Global());
  std::unique_ptr<ScopedPersistent<v8::Object>> weak_persistent =
      WTF::WrapUnique(new ScopedPersistent<v8::Object>);
  weak_persistent->Set(isolate_, wrapper);
  weak_persistent->SetPhantom();
  wrappers_.push_back(std::move(weak_persistent));
  DCHECK(wrapper->CreationContext() == context);
  return wrapper;
}

void ScriptPromisePropertyBase::ClearWrappers() {
  CheckThis();
  CheckWrappers();
  v8::HandleScope handle_scope(isolate_);
  for (WeakPersistentSet::iterator i = wrappers_.begin(); i != wrappers_.end();
       ++i) {
    v8::Local<v8::Object> wrapper = (*i)->NewLocal(isolate_);
    if (!wrapper.IsEmpty()) {
      v8::Context::Scope scope(wrapper->CreationContext());
      // TODO(peria): Use deleteProperty() if http://crbug.com/v8/6227 is fixed.
      ResolverSymbol().Set(wrapper, v8::Undefined(isolate_));
      PromiseSymbol().Set(wrapper, v8::Undefined(isolate_));
    }
  }
  wrappers_.clear();
}

void ScriptPromisePropertyBase::CheckThis() {
  CHECK(this);
}

void ScriptPromisePropertyBase::CheckWrappers() {
  for (WeakPersistentSet::iterator i = wrappers_.begin(); i != wrappers_.end();
       ++i) {
    CHECK(*i);
  }
}

V8PrivateProperty::Symbol ScriptPromisePropertyBase::PromiseSymbol() {
  switch (name_) {
#define P(Interface, Name)                                     \
  case Name:                                                   \
    return V8PrivateProperty::V8_PRIVATE_PROPERTY_GETTER_NAME( \
        Interface, Name##Promise)(isolate_);

    SCRIPT_PROMISE_PROPERTIES(P)

#undef P
  }
  NOTREACHED();
  return V8PrivateProperty::GetSymbol(isolate_, "noPromise");
}

V8PrivateProperty::Symbol ScriptPromisePropertyBase::ResolverSymbol() {
  switch (name_) {
#define P(Interface, Name)                                     \
  case Name:                                                   \
    return V8PrivateProperty::V8_PRIVATE_PROPERTY_GETTER_NAME( \
        Interface, Name##Resolver)(isolate_);

    SCRIPT_PROMISE_PROPERTIES(P)

#undef P
  }
  NOTREACHED();
  return V8PrivateProperty::GetSymbol(isolate_, "noResolver");
}

DEFINE_TRACE(ScriptPromisePropertyBase) {
  ContextClient::Trace(visitor);
}

}  // namespace blink
