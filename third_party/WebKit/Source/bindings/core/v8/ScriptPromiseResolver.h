// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptPromiseResolver_h
#define ScriptPromiseResolver_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SuspendableObject.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/heap/SelfKeepAlive.h"
#include "v8/include/v8.h"

namespace blink {

// This class wraps v8::Promise::Resolver and provides the following
// functionalities.
//  - A ScriptPromiseResolver retains a ScriptState. A caller
//    can call resolve or reject from outside of a V8 context.
//  - This class is an SuspendableObject and keeps track of the associated
//    ExecutionContext state. When the ExecutionContext is suspended,
//    resolve or reject will be delayed. When it is stopped, resolve or reject
//    will be ignored.
class CORE_EXPORT ScriptPromiseResolver
    : public GarbageCollectedFinalized<ScriptPromiseResolver>,
      public SuspendableObject {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptPromiseResolver);
  WTF_MAKE_NONCOPYABLE(ScriptPromiseResolver);

 public:
  static ScriptPromiseResolver* create(ScriptState* scriptState) {
    ScriptPromiseResolver* resolver = new ScriptPromiseResolver(scriptState);
    resolver->suspendIfNeeded();
    return resolver;
  }

#if DCHECK_IS_ON()
  // Eagerly finalized so as to ensure valid access to getExecutionContext()
  // from the destructor's assert.
  EAGERLY_FINALIZE();

  ~ScriptPromiseResolver() override {
    // This assertion fails if:
    //  - promise() is called at least once and
    //  - this resolver is destructed before it is resolved, rejected,
    //    detached, the V8 isolate is terminated or the associated
    //    ExecutionContext is stopped.
    ASSERT(m_state == Detached || !m_isPromiseCalled ||
           !getScriptState()->contextIsValid() || !getExecutionContext() ||
           getExecutionContext()->isContextDestroyed());
  }
#endif

  // Anything that can be passed to toV8 can be passed to this function.
  template <typename T>
  void resolve(T value) {
    resolveOrReject(value, Resolving);
  }

  // Anything that can be passed to toV8 can be passed to this function.
  template <typename T>
  void reject(T value) {
    resolveOrReject(value, Rejecting);
  }

  void resolve() { resolve(ToV8UndefinedGenerator()); }
  void reject() { reject(ToV8UndefinedGenerator()); }

  ScriptState* getScriptState() { return m_scriptState.get(); }

  // Note that an empty ScriptPromise will be returned after resolve or
  // reject is called.
  ScriptPromise promise() {
#if DCHECK_IS_ON()
    m_isPromiseCalled = true;
#endif
    return m_resolver.promise();
  }

  ScriptState* getScriptState() const { return m_scriptState.get(); }

  // SuspendableObject implementation.
  void suspend() override;
  void resume() override;
  void contextDestroyed(ExecutionContext*) override { detach(); }

  // Calling this function makes the resolver release its internal resources.
  // That means the associated promise will never be resolved or rejected
  // unless it's already been resolved or rejected.
  // Do not call this function unless you truly need the behavior.
  void detach();

  // Once this function is called this resolver stays alive while the
  // promise is pending and the associated ExecutionContext isn't stopped.
  void keepAliveWhilePending();

  DECLARE_VIRTUAL_TRACE();

 protected:
  // You need to call suspendIfNeeded after the construction because
  // this is an SuspendableObject.
  explicit ScriptPromiseResolver(ScriptState*);

 private:
  typedef ScriptPromise::InternalResolver Resolver;
  enum ResolutionState {
    Pending,
    Resolving,
    Rejecting,
    Detached,
  };

  template <typename T>
  void resolveOrReject(T value, ResolutionState newState) {
    if (m_state != Pending || !getScriptState()->contextIsValid() ||
        !getExecutionContext() || getExecutionContext()->isContextDestroyed())
      return;
    ASSERT(newState == Resolving || newState == Rejecting);
    m_state = newState;

    ScriptState::Scope scope(m_scriptState.get());

    // Calling ToV8 in a ScriptForbiddenScope will trigger a RELEASE_ASSERT and
    // cause a crash. ToV8 just invokes a constructor for wrapper creation,
    // which is safe (no author script can be run). Adding AllowUserAgentScript
    // directly inside createWrapper could cause a perf impact (calling
    // isMainThread() every time a wrapper is created is expensive). Ideally,
    // resolveOrReject shouldn't be called inside a ScriptForbiddenScope.
    {
      ScriptForbiddenScope::AllowUserAgentScript allowScript;
      m_value.set(m_scriptState->isolate(),
                  ToV8(value, m_scriptState->context()->Global(),
                       m_scriptState->isolate()));
    }

    if (getExecutionContext()->isContextSuspended()) {
      // Retain this object until it is actually resolved or rejected.
      keepAliveWhilePending();
      return;
    }
    // TODO(esprehn): This is a hack, instead we should RELEASE_ASSERT that
    // script is allowed, and v8 should be running the entry hooks below and
    // crashing if script is forbidden. We should then audit all users of
    // ScriptPromiseResolver and the related specs and switch to an async
    // resolve.
    // See: http://crbug.com/663476
    if (ScriptForbiddenScope::isScriptForbidden()) {
      m_timer.startOneShot(0, BLINK_FROM_HERE);
      return;
    }
    resolveOrRejectImmediately();
  }

  void resolveOrRejectImmediately();
  void onTimerFired(TimerBase*);

  ResolutionState m_state;
  const RefPtr<ScriptState> m_scriptState;
  TaskRunnerTimer<ScriptPromiseResolver> m_timer;
  Resolver m_resolver;
  ScopedPersistent<v8::Value> m_value;

  // To support keepAliveWhilePending(), this object needs to keep itself
  // alive while in that state.
  SelfKeepAlive<ScriptPromiseResolver> m_keepAlive;

#if DCHECK_IS_ON()
  // True if promise() is called.
  bool m_isPromiseCalled = false;
#endif
};

}  // namespace blink

#endif  // ScriptPromiseResolver_h
