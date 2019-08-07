// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/worker_task_queue.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/workers/experimental/task.h"
#include "third_party/blink/renderer/core/workers/experimental/thread_pool.h"

namespace blink {

WorkerTaskQueue* WorkerTaskQueue::Create(ExecutionContext* context,
                                         const String& type,
                                         ExceptionState& exception_state) {
  DCHECK(context->IsContextThread());

  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This frame is already detached.");
    return nullptr;
  }

  auto* document = DynamicTo<Document>(context);
  if (!document) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "WorkerTaskQueue can only be constructed from a document.");
    return nullptr;
  }
  DCHECK(type == "user-interaction" || type == "background");
  TaskType task_type = type == "user-interaction" ? TaskType::kUserInteraction
                                                  : TaskType::kIdleTask;
  return MakeGarbageCollected<WorkerTaskQueue>(document, task_type);
}

WorkerTaskQueue::WorkerTaskQueue(Document* document, TaskType task_type)
    : document_(document), task_type_(task_type) {}

ScriptPromise WorkerTaskQueue::postFunction(
    ScriptState* script_state,
    V8Function* function,
    AbortSignal* signal,
    const Vector<ScriptValue>& arguments,
    ExceptionState& exception_state) {
  DCHECK(document_->IsContextThread());

  if (document_->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This frame is already detached");
    return ScriptPromise();
  }

  Task* task = MakeGarbageCollected<Task>(
      script_state, ThreadPool::From(*document_), function, arguments,
      task_type_, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (signal)
    signal->AddAlgorithm(WTF::Bind(&Task::cancel, WrapWeakPersistent(task)));
  return task->result(script_state, exception_state);
}

Task* WorkerTaskQueue::postTask(ScriptState* script_state,
                                V8Function* function,
                                const Vector<ScriptValue>& arguments,
                                ExceptionState& exception_state) {
  DCHECK(document_->IsContextThread());

  if (document_->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This frame is already detached");
    return nullptr;
  }

  Task* task = MakeGarbageCollected<Task>(
      script_state, ThreadPool::From(*document_), function, arguments,
      task_type_, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return task;
}

void WorkerTaskQueue::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(document_);
}

}  // namespace blink
