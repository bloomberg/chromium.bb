// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AbortSignal;
class ScriptState;
class V8TaskQueuePostCallback;

// This class corresponds to the ScriptedTaskQueue interface.
class CORE_EXPORT ScriptedTaskQueue final : public ScriptWrappable,
                                            public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ScriptedTaskQueue);

 public:
  static ScriptedTaskQueue* Create(ExecutionContext* context,
                                   TaskType task_type) {
    return MakeGarbageCollected<ScriptedTaskQueue>(context, task_type);
  }

  explicit ScriptedTaskQueue(ExecutionContext*, TaskType);

  using CallbackId = int;

  ScriptPromise postTask(ScriptState*,
                         V8TaskQueuePostCallback* callback,
                         AbortSignal*);

  void CallbackFired(CallbackId id);

  void Trace(Visitor*) override;

 private:
  // ContextLifecycleObserver interface.
  void ContextDestroyed(ExecutionContext*) override;

  void AbortTask(CallbackId id);

  class WrappedCallback;
  HeapHashMap<CallbackId, Member<WrappedCallback>> pending_tasks_;
  CallbackId next_callback_id_ = 1;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_H_
