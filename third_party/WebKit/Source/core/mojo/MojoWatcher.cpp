// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/mojo/MojoWatcher.h"

#include "bindings/core/v8/MojoWatchCallback.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/mojo/MojoHandleSignals.h"
#include "platform/WebTaskRunner.h"

namespace blink {

static void runWatchCallback(MojoWatchCallback* callback,
                             ScriptWrappable* wrappable,
                             MojoResult result) {
  callback->call(wrappable, result);
}

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

MojoWatcher::MojoWatcher(ExecutionContext* context, MojoWatchCallback* callback)
    : ContextLifecycleObserver(context),
      m_taskRunner(TaskRunnerHelper::get(TaskType::UnspecedTimer, context)),
      m_callback(this, callback) {}

MojoWatcher::~MojoWatcher() {
  DCHECK(!m_handle.is_valid());
}

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
      MojoWatch(handle.value(), signals, &MojoWatcher::onHandleReady,
                reinterpret_cast<uintptr_t>(this));
  if (result == MOJO_RESULT_OK) {
    m_handle = handle;
  }
  return result;
}

MojoResult MojoWatcher::cancel() {
  if (!m_handle.is_valid())
    return MOJO_RESULT_OK;

  MojoResult result =
      MojoCancelWatch(m_handle.value(), reinterpret_cast<uintptr_t>(this));
  m_handle = mojo::Handle();
  return result;
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

void MojoWatcher::onHandleReady(uintptr_t context,
                                MojoResult result,
                                MojoHandleSignalsState,
                                MojoWatchNotificationFlags) {
  // It is safe to assume the MojoWatcher still exists because this
  // callback will never be run after MojoWatcher destructor,
  // which cancels the watch.
  MojoWatcher* watcher = reinterpret_cast<MojoWatcher*>(context);
  watcher->m_taskRunner->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&MojoWatcher::runReadyCallback,
                      wrapCrossThreadWeakPersistent(watcher), result));
}

void MojoWatcher::runReadyCallback(MojoResult result) {
  // Ignore callbacks if not watching.
  if (!m_handle.is_valid())
    return;

  // MOJO_RESULT_CANCELLED indicates that the handle has been closed, in which
  // case watch has been implicitly cancelled. There is no need to explicitly
  // cancel the watch.
  if (result == MOJO_RESULT_CANCELLED)
    m_handle = mojo::Handle();

  runWatchCallback(m_callback, this, result);
}

}  // namespace blink
