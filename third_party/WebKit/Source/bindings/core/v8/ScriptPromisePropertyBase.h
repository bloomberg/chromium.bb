// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptPromisePropertyBase_h
#define ScriptPromisePropertyBase_h

#include <memory>

#include "bindings/core/v8/ScriptPromise.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/ScriptPromiseProperties.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class ExecutionContext;
class ScriptState;

// TODO(yhirano): Remove NEVER_INLINE once we find the cause of crashes.
class CORE_EXPORT ScriptPromisePropertyBase
    : public GarbageCollectedFinalized<ScriptPromisePropertyBase>,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptPromisePropertyBase);

 public:
  virtual ~ScriptPromisePropertyBase();

  enum Name {
#define P(Unused, Name) Name,
    SCRIPT_PROMISE_PROPERTIES(P)
#undef P
  };

  enum State {
    kPending,
    kResolved,
    kRejected,
  };
  State GetState() const { return state_; }

  ScriptPromise Promise(DOMWrapperWorld&);

  virtual void Trace(blink::Visitor*);

 protected:
  ScriptPromisePropertyBase(ExecutionContext*, Name);

  void ResolveOrReject(State target_state);

  // ScriptPromiseProperty overrides these to wrap the holder,
  // rejected value and resolved value. The
  // ScriptPromisePropertyBase caller will enter the V8Context for
  // the property's execution context and the world it is
  // creating/settling promises in; the implementation should use
  // this context.
  virtual v8::Local<v8::Object> Holder(
      v8::Isolate*,
      v8::Local<v8::Object> creation_context) = 0;
  virtual v8::Local<v8::Value> ResolvedValue(
      v8::Isolate*,
      v8::Local<v8::Object> creation_context) = 0;
  virtual v8::Local<v8::Value> RejectedValue(
      v8::Isolate*,
      v8::Local<v8::Object> creation_context) = 0;

  NEVER_INLINE void ResetBase();

 private:
  typedef Vector<std::unique_ptr<ScopedPersistent<v8::Object>>>
      WeakPersistentSet;

  void ResolveOrRejectInternal(v8::Local<v8::Promise::Resolver>);
  v8::Local<v8::Object> EnsureHolderWrapper(ScriptState*);
  NEVER_INLINE void ClearWrappers();
  // TODO(yhirano): Remove these functions once we find the cause of crashes.
  NEVER_INLINE void CheckThis();
  NEVER_INLINE void CheckWrappers();

  V8PrivateProperty::Symbol PromiseSymbol();
  V8PrivateProperty::Symbol ResolverSymbol();

  v8::Isolate* isolate_;
  Name name_;
  State state_;

  WeakPersistentSet wrappers_;
};

}  // namespace blink

#endif  // ScriptPromisePropertyBase_h
