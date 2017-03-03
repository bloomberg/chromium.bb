// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptPromiseResolver.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/InspectorInstrumentation.h"

namespace blink {

ScriptPromiseResolver::ScriptPromiseResolver(ScriptState* scriptState)
    : SuspendableObject(scriptState->getExecutionContext()),
      m_state(Pending),
      m_scriptState(scriptState),
      m_timer(TaskRunnerHelper::get(TaskType::Microtask, getExecutionContext()),
              this,
              &ScriptPromiseResolver::onTimerFired),
      m_resolver(scriptState)
{
  if (getExecutionContext()->isContextDestroyed()) {
    m_state = Detached;
    m_resolver.clear();
  }
  probe::asyncTaskScheduled(getExecutionContext(), "Promise", this);
}

void ScriptPromiseResolver::suspend() {
  m_timer.stop();
}

void ScriptPromiseResolver::resume() {
  if (m_state == Resolving || m_state == Rejecting)
    m_timer.startOneShot(0, BLINK_FROM_HERE);
}

void ScriptPromiseResolver::detach() {
  if (m_state == Detached)
    return;
  m_timer.stop();
  m_state = Detached;
  m_resolver.clear();
  m_value.clear();
  m_keepAlive.clear();
  probe::asyncTaskCanceled(getExecutionContext(), this);
}

void ScriptPromiseResolver::keepAliveWhilePending() {
  // keepAliveWhilePending() will be called twice if the resolver
  // is created in a suspended execution context and the resolver
  // is then resolved/rejected while in that suspended state.
  if (m_state == Detached || m_keepAlive)
    return;

  // Keep |this| around while the promise is Pending;
  // see detach() for the dual operation.
  m_keepAlive = this;
}

void ScriptPromiseResolver::onTimerFired(TimerBase*) {
  ASSERT(m_state == Resolving || m_state == Rejecting);
  if (!getScriptState()->contextIsValid()) {
    detach();
    return;
  }

  ScriptState::Scope scope(m_scriptState.get());
  resolveOrRejectImmediately();
}

void ScriptPromiseResolver::resolveOrRejectImmediately() {
  DCHECK(!getExecutionContext()->isContextDestroyed());
  DCHECK(!getExecutionContext()->isContextSuspended());
  {
    probe::AsyncTask asyncTask(getExecutionContext(), this);
    if (m_state == Resolving) {
      m_resolver.resolve(m_value.newLocal(m_scriptState->isolate()));
    } else {
      ASSERT(m_state == Rejecting);
      m_resolver.reject(m_value.newLocal(m_scriptState->isolate()));
    }
  }
  detach();
}

DEFINE_TRACE(ScriptPromiseResolver) {
  SuspendableObject::trace(visitor);
}

}  // namespace blink
