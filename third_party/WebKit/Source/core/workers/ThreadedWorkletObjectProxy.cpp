// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletObjectProxy.h"

#include <memory>
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<ThreadedWorkletObjectProxy> ThreadedWorkletObjectProxy::Create(
    ThreadedWorkletMessagingProxy* messaging_proxy_weak_ptr,
    ParentFrameTaskRunners* parent_frame_task_runners) {
  DCHECK(messaging_proxy_weak_ptr);
  return WTF::WrapUnique(new ThreadedWorkletObjectProxy(
      messaging_proxy_weak_ptr, parent_frame_task_runners));
}

ThreadedWorkletObjectProxy::~ThreadedWorkletObjectProxy() {}

void ThreadedWorkletObjectProxy::EvaluateScript(const String& source,
                                                const KURL& script_url,
                                                WorkerThread* worker_thread) {
  ThreadedWorkletGlobalScope* global_scope =
      ToThreadedWorkletGlobalScope(worker_thread->GlobalScope());
  global_scope->ScriptController()->Evaluate(
      ScriptSourceCode(source, script_url));
}

ThreadedWorkletObjectProxy::ThreadedWorkletObjectProxy(
    ThreadedWorkletMessagingProxy* messaging_proxy_weak_ptr,
    ParentFrameTaskRunners* parent_frame_task_runners)
    : ThreadedObjectProxyBase(parent_frame_task_runners),
      messaging_proxy_weak_ptr_(messaging_proxy_weak_ptr) {}

CrossThreadWeakPersistent<ThreadedMessagingProxyBase>
ThreadedWorkletObjectProxy::MessagingProxyWeakPtr() {
  return messaging_proxy_weak_ptr_;
}

}  // namespace blink
