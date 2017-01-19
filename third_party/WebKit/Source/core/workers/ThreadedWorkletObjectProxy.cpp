// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletObjectProxy.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "core/workers/WorkerThread.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<ThreadedWorkletObjectProxy> ThreadedWorkletObjectProxy::create(
    const WeakPtr<ThreadedWorkletMessagingProxy>& messagingProxyWeakPtr,
    ParentFrameTaskRunners* parentFrameTaskRunners) {
  DCHECK(messagingProxyWeakPtr);
  return WTF::wrapUnique(new ThreadedWorkletObjectProxy(
      messagingProxyWeakPtr, parentFrameTaskRunners));
}

ThreadedWorkletObjectProxy::~ThreadedWorkletObjectProxy() {}

void ThreadedWorkletObjectProxy::evaluateScript(const String& source,
                                                const KURL& scriptURL,
                                                WorkerThread* workerThread) {
  ThreadedWorkletGlobalScope* globalScope =
      toThreadedWorkletGlobalScope(workerThread->globalScope());
  globalScope->scriptController()->evaluate(
      ScriptSourceCode(source, scriptURL));
}

ThreadedWorkletObjectProxy::ThreadedWorkletObjectProxy(
    const WeakPtr<ThreadedWorkletMessagingProxy>& messagingProxyWeakPtr,
    ParentFrameTaskRunners* parentFrameTaskRunners)
    : ThreadedObjectProxyBase(parentFrameTaskRunners),
      m_messagingProxyWeakPtr(messagingProxyWeakPtr) {}

WeakPtr<ThreadedMessagingProxyBase>
ThreadedWorkletObjectProxy::messagingProxyWeakPtr() {
  return m_messagingProxyWeakPtr;
}

}  // namespace blink
