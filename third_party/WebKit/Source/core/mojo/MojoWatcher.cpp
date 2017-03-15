// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/mojo/MojoWatcher.h"

#include "bindings/core/v8/MojoWatchCallback.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/mojo/MojoHandleSignals.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"

namespace blink {

static void runWatchCallback(MojoWatchCallback* callback,
                             ScriptWrappable* wrappable,
                             MojoResult result) {
  callback->call(wrappable, result);
}

// static
MojoWatcher* MojoWatcher::create(mojo::Handle handle,
                                 const MojoHandleSignals& signalsDict,
                                 MojoWatchCallback* callback,
                                 ExecutionContext* context) {
  MojoWatcher* watcher = new MojoWatcher(context, callback);
  MojoResult result = watcher->watch(handle, signalsDict);
  // TODO(alokp): Consider raising an exception.
  // Current clients expect to recieve the initial error returned by MojoWatch
  // via watch callback.
  //
  // Note that the usage of wrapPersistent is intentional so that the intial
  // error is guaranteed to be reported to the client in case where the given
  // handle is invalid and garbage collection happens before the callback
  // is scheduled.
  if (result != MOJO_RESULT_OK) {
    watcher->m_taskRunner->postTask(
        BLINK_FROM_HERE, WTF::bind(&runWatchCallback, wrapPersistent(callback),
                                   wrapPersistent(watcher), result));
  }
  return watcher;
}

MojoWatcher::~MojoWatcher() {
  DCHECK(!m_handle.is_valid());
}

MojoResult MojoWatcher::cancel() {
  if (!m_watcherHandle.is_valid())
    return MOJO_RESULT_INVALID_ARGUMENT;

  m_watcherHandle.reset();
  return MOJO_RESULT_OK;
}

DEFINE_TRACE(MojoWatcher) {
  visitor->trace(m_callback);
  ContextLifecycleObserver::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(MojoWatcher) {
  visitor->traceWrappers(m_callback);
}

bool MojoWatcher::hasPendingActivity() const {
  return m_handle.is_valid();
}

void MojoWatcher::contextDestroyed(ExecutionContext*) {
  cancel();
}

MojoWatcher::MojoWatcher(ExecutionContext* context, MojoWatchCallback* callback)
    : ContextLifecycleObserver(context),
      m_taskRunner(TaskRunnerHelper::get(TaskType::UnspecedTimer, context)),
      m_callback(this, callback) {}

MojoResult MojoWatcher::watch(mojo::Handle handle,
                              const MojoHandleSignals& signalsDict) {
  ::MojoHandleSignals signals = MOJO_HANDLE_SIGNAL_NONE;
  if (signalsDict.readable())
    signals |= MOJO_HANDLE_SIGNAL_READABLE;
  if (signalsDict.writable())
    signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  if (signalsDict.peerClosed())
    signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;

  MojoResult result =
      mojo::CreateWatcher(&MojoWatcher::onHandleReady, &m_watcherHandle);
  DCHECK_EQ(MOJO_RESULT_OK, result);

  result = MojoWatch(m_watcherHandle.get().value(), handle.value(), signals,
                     reinterpret_cast<uintptr_t>(this));
  if (result != MOJO_RESULT_OK)
    return result;

  m_handle = handle;

  MojoResult readyResult;
  result = arm(&readyResult);
  if (result == MOJO_RESULT_OK)
    return result;

  // We couldn't arm the watcher because the handle is already ready to
  // trigger a success notification. Post a notification manually.
  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  m_taskRunner->postTask(BLINK_FROM_HERE,
                         WTF::bind(&MojoWatcher::runReadyCallback,
                                   wrapPersistent(this), readyResult));
  return MOJO_RESULT_OK;
}

MojoResult MojoWatcher::arm(MojoResult* readyResult) {
  // Nothing to do if the watcher is inactive.
  if (!m_handle.is_valid())
    return MOJO_RESULT_OK;

  uint32_t numReadyContexts = 1;
  uintptr_t readyContext;
  MojoResult localReadyResult;
  MojoHandleSignalsState readySignals;
  MojoResult result =
      MojoArmWatcher(m_watcherHandle.get().value(), &numReadyContexts,
                     &readyContext, &localReadyResult, &readySignals);
  if (result == MOJO_RESULT_OK)
    return MOJO_RESULT_OK;

  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  DCHECK_EQ(1u, numReadyContexts);
  DCHECK_EQ(reinterpret_cast<uintptr_t>(this), readyContext);
  *readyResult = localReadyResult;
  return result;
}

void MojoWatcher::onHandleReady(uintptr_t context,
                                MojoResult result,
                                MojoHandleSignalsState,
                                MojoWatcherNotificationFlags) {
  // It is safe to assume the MojoWatcher still exists. It stays alive at least
  // as long as |m_handle| is valid, and |m_handle| is only reset after we
  // dispatch a |MOJO_RESULT_CANCELLED| notification. That is always the last
  // notification received by this callback.
  MojoWatcher* watcher = reinterpret_cast<MojoWatcher*>(context);
  watcher->m_taskRunner->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&MojoWatcher::runReadyCallback,
                      wrapCrossThreadWeakPersistent(watcher), result));
}

void MojoWatcher::runReadyCallback(MojoResult result) {
  if (result == MOJO_RESULT_CANCELLED) {
    // Last notification.
    m_handle = mojo::Handle();

    // Only dispatch to the callback if this cancellation was implicit due to
    // |m_handle| closure. If it was explicit, |m_watcherHandle| has already
    // been reset.
    if (m_watcherHandle.is_valid()) {
      m_watcherHandle.reset();
      runWatchCallback(m_callback, this, result);
    }
    return;
  }

  // Ignore callbacks if not watching.
  if (!m_watcherHandle.is_valid())
    return;

  runWatchCallback(m_callback, this, result);

  // Rearm the watcher so another notification can fire.
  //
  // TODO(rockot): MojoWatcher should expose some better approximation of the
  // new watcher API, including explicit add and removal of handles from the
  // watcher, as well as explicit arming.
  MojoResult readyResult;
  MojoResult armResult = arm(&readyResult);
  if (armResult == MOJO_RESULT_OK)
    return;

  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, armResult);

  m_taskRunner->postTask(BLINK_FROM_HERE,
                         WTF::bind(&MojoWatcher::runReadyCallback,
                                   wrapWeakPersistent(this), readyResult));
}

}  // namespace blink
